#include <array>
#include <functional>
#include <thread>
#include <chrono>
#include <iostream>
#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"
#include "vtfCUtils.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"
#include "vtfThreadSafeLogger.hpp"
#include "vtfFormatUtils.hpp"

#include <optional>

#define MAX_BACK_BUFFER_COUNT 2

namespace vtf
{

GlfwInitializerFinalizer::GlfwInitializerFinalizer(bool initialize)
	: m_initialize(initialize)
{
	if (initialize)
	{
		ASSERTMSG(GLFW_TRUE == glfwInit(), "Failed to initialize GLFW library");
	}
}

GlfwInitializerFinalizer::~GlfwInitializerFinalizer ()
{
	if (m_initialize) glfwTerminate();
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Destructor " << __func__
				  << (m_initialize ? " calls glfwTerminate()" : "") << std::endl;
	}
}

CanvasContext::~CanvasContext ()
{
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Destructor " << __func__ << ' '
				  << cc_device << '(' << cc_device.use_count() << ") "
				  << cc_physicalDevice << '(' << cc_physicalDevice.use_count() << ") "
				  << cc_instance << '(' << cc_instance.use_count() << ") "
				  << std::endl;
	}
}

Canvas::~Canvas	()
{
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Destructor " << __func__ << std::endl;
	}
}

int Canvas::m_drawTrigger;
const CanvasStyle Canvas::DefaultStyle
{
	800u,	// width
	600u,	// height
	0.0f,	// minDepth
	1.0f,	// maxDepth
	true,	// visible
	true,	// resizable
	0		// surfaceFormatFlags
};

strings			getGlfwRequiredInstanceExtensions ();
ZGLFWwindowPtr	createWindow (const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer);
ZSurfaceKHR		createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window);
ZGLFWwindowPtr	updateWindow (ZGLFWwindowPtr window, const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer);

CanvasContext::CanvasContext (add_cptr<char>		appName,
							  add_cref<Version>		apiVersion,
							  add_cref<strings>		instanceLayers,
							  add_cref<strings>		instanceExtensions,
							  add_cref<strings>		deviceExtensions,
							  OnEnablingFeatures	onEnablingFeatures,
							  bool					enableDebugPrintf,
							  add_cref<CanvasStyle>	style,
							  add_ptr<Canvas>		canvas)
	: cc_callbacks		(getAllocationCallbacks())
	, cc_debugMessenger	(VK_NULL_HANDLE)
	, cc_debugReport	(VK_NULL_HANDLE)
	, cc_instance		(getSharedInstance() | ([&,this]() -> ZInstance {
							return createInstance(appName, cc_callbacks, instanceLayers,
												  mergeStringsDistinct(getGlfwRequiredInstanceExtensions(), instanceExtensions),
												  &cc_debugMessenger, this, &cc_debugReport, this, apiVersion, enableDebugPrintf);
							 }))
	, cc_window			(createWindow(style, appName, canvas))
	, cc_surface		(createSurface(cc_instance, cc_callbacks, cc_window))
	, cc_physicalDevice	(getSharedInstance().select(getSharedPhysicalDevice(), ([&,this]() -> ZPhysicalDevice {
								return selectPhysicalDevice(make_signed(getGlobalAppFlags().physicalDeviceIndex),
															cc_instance, deviceExtensions, cc_surface);
							 })))
	, cc_device			(getSharedInstance().select(getSharedDevice(), ([&,this]() -> ZDevice {
								return createLogicalDevice(cc_physicalDevice, onEnablingFeatures, cc_surface, enableDebugPrintf);
							 })))
{
}

Canvas::Canvas	(add_cptr<char>			appName,
				 add_cref<strings>		instanceLayers,
				 add_cref<strings>		instanceExtensions,
				 add_cref<strings>		deviceExtensions,
				 add_cref<CanvasStyle>	canvasStyle,
				 OnEnablingFeatures		onEnablingFeatures,
				 bool					enableDebugPrintf,
				 add_cref<Version>		apiVersion)
	: GlfwInitializerFinalizer()
	, CanvasContext(appName, apiVersion, instanceLayers, instanceExtensions, deviceExtensions, onEnablingFeatures, enableDebugPrintf, canvasStyle, this)
	, VulkanContext	(cc_callbacks, cc_debugMessenger, cc_debugReport, cc_instance, cc_physicalDevice, cc_device)
	// beginning references initialization
	, window					(cc_window)
	, surface					(cc_surface)
	, surfaceDetails			(m_surfaceDetails)
	, surfaceFormatIndex		(m_surfaceFormatIndex)
	, surfaceFormat				(m_surfaceFormat)
	, style						(canvasStyle)
	, width						(m_width)
	, height					(m_height)
	, presentQueue				(m_presentQueue)
	// end of references initialization
	, m_surfaceDetails			()
	, m_surfaceFormatIndex		(INVALID_UINT32)
	, m_width					(style.width)
	, m_height					(style.height)
	, m_presentQueue			(queueSupportSwapchain(graphicsQueue)
									? graphicsQueue
									: deviceGetNextQueue(device, VK_QUEUE_GRAPHICS_BIT, true))
	, m_events					(new GLFWEvents(*this))
	, m_timerUserData			(nullptr)
	, m_timerPeriodMS			(0)
{
	m_surfaceDetails.update(physicalDevice, surface);
	m_surfaceFormatIndex = m_surfaceDetails.selectFormat(physicalDevice, canvasStyle.surfaceFormatFlags);
	ASSERTMSG(INVALID_UINT32 != m_surfaceFormatIndex, "Unable to find surface format with desired features");
	m_surfaceFormat = m_surfaceDetails.formats[m_surfaceFormatIndex].format;

	if (getGlobalAppFlags().verbose)
	{
		logger << "[INFO] Available surface formats: " << m_surfaceDetails.formats.size() << '\n';
		for (uint32_t i = 0; i < uint32_t(m_surfaceDetails.formats.size()); ++i)
			logger << "\tFormat[" << i << "] " << formatGetString(m_surfaceDetails.formats[i].format) << '\n';
		logger << "[INFO} Most suitable surface format is " << formatGetString(m_surfaceDetails.formats[m_surfaceFormatIndex].format);
		logger << std::endl;
	}
}

strings getGlfwRequiredInstanceExtensions ()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	strings extensions(glfwExtensionCount);
	for (uint32_t glfwExtIndex = 0; glfwExtIndex < glfwExtensionCount; ++glfwExtIndex)
		extensions[glfwExtIndex] = glfwExtensions[glfwExtIndex];
	return extensions;
}

ZGLFWwindowPtr createWindow (const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer)
{
	const char* caption = title ? title : "Vulkan";
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// invisible at start, if style.visible is set the call glfwShowWindows() later
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, (style.resizable ? GLFW_TRUE : GLFW_FALSE));
	GLFWwindow* pWindow = glfwCreateWindow(static_cast<int>(style.width), static_cast<int>(style.height), caption, nullptr, nullptr);
	ZGLFWwindowPtr window = ZGLFWwindowPtr::create(pWindow);
	ASSERTMSG(window.has_handle(), "Failed to create GLFW window");
	glfwSetWindowUserPointer(*window, windowUserPointer);
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] " << __func__ << ' ' << window
				  << ", Caption: " << std::quoted(caption)
				  << ", windowUserPointer: " << windowUserPointer
				  << std::endl;
	}
	return window;
}

ZGLFWwindowPtr updateWindow (ZGLFWwindowPtr window, const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer)
{
	const char* caption = title ? title : "Vulkan";
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, (style.resizable ? GLFW_TRUE : GLFW_FALSE));
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] " << __func__ << ' ' << window
				  << ", Caption: " << std::quoted(caption)
				  << ", windowUserPointer: " << windowUserPointer
				  << std::endl;
	}
	if (window.has_handle() == false) return window;
	glfwSetWindowSize(*window, static_cast<int>(style.width), static_cast<int>(style.height));
	glfwSetWindowTitle(*window, caption);
	glfwSetWindowUserPointer(*window, windowUserPointer);
	return window;
}

ZSurfaceKHR	createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VKASSERT2(glfwCreateWindowSurface(*instance, *window, callbacks, &surface));
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] " << __func__ << ' ' << surface << ' ' << window << std::endl;
	}
	return ZSurfaceKHR::create(surface, instance, callbacks, window.asSharedPtr());
}

Canvas::SurfaceDetails::SurfaceDetails ()
	: caps		()
	, formats	()
	, modes		()
{
}

void Canvas::SurfaceDetails::update (ZPhysicalDevice physDevice, ZSurfaceKHR surfaceKHR)
{
	VKASSERT2(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physDevice, *surfaceKHR, &caps));

	uint32_t formatCount = 0;;
	VKASSERT2(vkGetPhysicalDeviceSurfaceFormatsKHR(*physDevice, *surfaceKHR, &formatCount, nullptr));
	ASSERTION(formatCount != 0);
	formats.resize(formatCount);
	VKASSERT2(vkGetPhysicalDeviceSurfaceFormatsKHR(*physDevice, *surfaceKHR, &formatCount, formats.data()));

	uint32_t presentModeCount = 0;
	VKASSERT2(vkGetPhysicalDeviceSurfacePresentModesKHR(*physDevice, *surfaceKHR, &presentModeCount, nullptr));
	ASSERTION(presentModeCount != 0);
	modes.resize(presentModeCount);
	VKASSERT2(vkGetPhysicalDeviceSurfacePresentModesKHR(*physDevice, *surfaceKHR, &presentModeCount, modes.data()));
}

uint32_t Canvas::SurfaceDetails::selectFormat (ZPhysicalDevice physDevice, VkFormatFeatureFlags formatFlags)
{
	if (0 == formatFlags)
		return 0;

	for (uint32_t i = 0; i < uint32_t(formats.size()); ++i)
	{
		add_cref<VkSurfaceFormatKHR> format (formats.at(i));
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(*physDevice, format.format, &properties);
		if ((formatFlags & properties.optimalTilingFeatures) != 0)
			return i;
	}

	return INVALID_UINT32;
}

Canvas::BackBuffer::BackBuffer (ZCommandPool renderPool, ZCommandPool blitPool)
	: imageIndex		(INVALID_UINT32)
	, blitImage			()
	, blitCommand		(::vtf::createCommandBuffer(blitPool))
	, blitFence			(::vtf::createFence(blitPool.getParam<ZDevice>(), false))
	, acquireSemaphore	(::vtf::createSemaphore(renderPool.getParam<ZDevice>()))
	, renderSemaphore	(::vtf::createSemaphore(renderPool.getParam<ZDevice>()))
	, presentFence		(::vtf::createFence(renderPool.getParam<ZDevice>(), false))
	, renderCommand		(::vtf::createCommandBuffer(renderPool))
	, renderFence		(::vtf::createFence(renderPool.getParam<ZDevice>(), false))
	, threadIndex		(INVALID_UINT32)
	, threadID			(std::this_thread::get_id())
{
	ASSERTION(renderPool.getParam<ZDevice>() == blitPool.getParam<ZDevice>());
}

add_ref<Canvas::BackBuffer> Canvas::BackBuffer::operator=(add_cref<BackBuffer> src)
{
	imageIndex			= (src.imageIndex)		;
	blitImage			= (src.blitImage)		;
	blitCommand			= (src.blitCommand)		;
	blitFence			= (src.blitFence)		;
	acquireSemaphore	= (src.acquireSemaphore);
	renderSemaphore		= (src.renderSemaphore)	;
	presentFence		= (src.presentFence)	;
	renderCommand		= (src.renderCommand)	;
	renderFence			= (src.renderFence)		;
	threadIndex			= (src.threadIndex)		;
	threadID			= (src.threadID)		;

	return *this;
}

Canvas::BackBuffer::BackBuffer (add_cref<BackBuffer> src)
	: imageIndex		(src.imageIndex)
	, blitImage			(src.blitImage)
	, blitCommand		(src.blitCommand)
	, blitFence			(src.blitFence)
	, acquireSemaphore	(src.acquireSemaphore)
	, renderSemaphore	(src.renderSemaphore)
	, presentFence		(src.presentFence)
	, renderCommand		(src.renderCommand)
	, renderFence		(src.renderFence)
	, threadIndex		(src.threadIndex)
	, threadID			(src.threadID)
{
	*this = src;
}

uint32_t Canvas::getPresentQueueFamilyIndex () const
{
	return queueGetFamilyIndex(presentQueue);
}

Canvas::Swapchain::Swapchain (add_cref<Canvas> aCanvas)
	: canvas		(aCanvas)
	, handle		(m_handle)
	, viewport		(m_viewport)
	, scissor		(m_scissor)
	, extent		(m_extent)
	, images		(m_images)
	, framebuffers	(m_framebuffers)
	, bufferCount	(m_bufferCount)
	, refreshCount	(m_refreshCount)
	, renderPass	(m_renderPass)
	, m_handle		(VK_NULL_HANDLE)
	, m_images		()
	, m_framebuffers()
	, m_refreshCount()
	, m_viewport	()
	, m_scissor		()
	, m_extent		()
	, m_bufferCount	()
	, m_renderPass	()
{
	m_viewport.x		= 0.0f;
	m_viewport.y		= 0.0f;
	m_viewport.width	= static_cast<float>(canvas.width);
	m_viewport.height	= static_cast<float>(canvas.height);
	m_viewport.minDepth	= canvas.style.minDepth;
	m_viewport.maxDepth	= canvas.style.maxDepth;

	m_extent			= { canvas.width, canvas.height };

	m_scissor.offset	= {0, 0};
	m_scissor.extent	= m_extent;
}

Canvas::Swapchain::~Swapchain ()
{
	if (VK_NULL_HANDLE != m_handle)
	{
		vkDeviceWaitIdle(*canvas.device);

		destroyFramebuffers();

		vkDestroySwapchainKHR(*canvas.device, m_handle, canvas.callbacks);
		m_handle = VK_NULL_HANDLE;
	}

	if (getGlobalAppFlags().verbose > 9)
	{
		canvas.logger << "Destroing swapchain" << std::endl;
	}
}

void Canvas::Swapchain::destroyFramebuffers ()
{
	m_images.resize(0);
	m_framebuffers.resize(0);
}

// If rp has no handle then creates only images, otherwise views and framebuffers as well
void Canvas::Swapchain::createFramebuffers (ZRenderPass rp, uint32_t minImageCount)
{
	ASSERTION(m_handle != VK_NULL_HANDLE);

	std::vector<VkImage>		presentImages;
	const uint32_t				imageCount = enumerateSwapchainImages(*canvas.device, m_handle, presentImages);
	ASSERTION(imageCount >= minImageCount);
	const VkFormat				format = canvas.surfaceDetails.formats[canvas.surfaceFormatIndex].format;
	const ZImageUsageFlags		usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	minImageCount = std::min(minImageCount, imageCount);
	for (uint32_t i = 0; i < minImageCount; ++i)
	{
		ZNonDeletableImage image = ZNonDeletableImage::create(presentImages[i], canvas.device, format, m_extent.width, m_extent.height, usage);
		m_images.emplace_back(image);
		if (rp.has_handle())
		{
			ZImageView view = ::vtf::createImageView(image);
			m_framebuffers.emplace_back(createFramebuffer(rp, m_extent.width, m_extent.height, { view }));
		}
	}
}

void Canvas::Swapchain::recreate (ZRenderPass rp, uint32_t acquirableImageCount, uint32_t hintWidth, uint32_t hintHeight, bool force)
{
	VkSurfaceCapabilitiesKHR caps;
	VKASSERT2(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*canvas.physicalDevice, *canvas.surface, &caps));

	if (caps.currentExtent.width == INVALID_UINT32 || caps.currentExtent.height == INVALID_UINT32)
	{
		caps.currentExtent.width = hintWidth;
		caps.currentExtent.height = hintHeight;
	}

	if (caps.currentExtent.width < caps.minImageExtent.width)
		caps.currentExtent.width = caps.minImageExtent.width;
	else if (caps.currentExtent.width > caps.maxImageExtent.width)
		caps.currentExtent.width = caps.maxImageExtent.width;

	if (caps.currentExtent.height < caps.minImageExtent.height)
		caps.currentExtent.height = caps.minImageExtent.height;
	else if (caps.currentExtent.height > caps.maxImageExtent.height)
		caps.currentExtent.height = caps.maxImageExtent.height;

	if (!force && caps.currentExtent.width == m_extent.width && caps.currentExtent.height == m_extent.height)
	{
		return;
	}

	m_viewport.x		= 0.0f;
	m_viewport.y		= 0.0f;
	m_viewport.width	= static_cast<float>(caps.currentExtent.width);
	m_viewport.height	= static_cast<float>(caps.currentExtent.height);
	m_viewport.minDepth	= canvas.style.minDepth;
	m_viewport.maxDepth	= canvas.style.maxDepth;

	m_scissor.offset	= {0, 0};
	m_scissor.extent	= caps.currentExtent;

	m_extent			= caps.currentExtent;

	{
		uint32_t	minImageCount = acquirableImageCount + 1;
		// avoid caps.maxImageCount is 0, it means no limit
		if (caps.maxImageCount == 0)
			minImageCount = acquirableImageCount + 1;
		else if ((acquirableImageCount + 1) < caps.minImageCount)
			minImageCount = caps.minImageCount;
		else if ((acquirableImageCount + 1) > caps.maxImageCount)
			minImageCount = caps.maxImageCount;
		m_bufferCount = minImageCount;
	}

	ASSERTION(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	ASSERTION(caps.supportedTransforms & caps.currentTransform);
	ASSERTION(caps.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR));
	const VkCompositeAlphaFlagBitsKHR compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
														? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
														: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkPresentModeKHR mode = select_if(canvas.surfaceDetails.modes, VK_PRESENT_MODE_FIFO_KHR,
									  [](const auto& m) { return (m == VK_PRESENT_MODE_MAILBOX_KHR || m == VK_PRESENT_MODE_IMMEDIATE_KHR); });

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType				= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	//swapchainCreateInfo.pNext:			default
	//swapchainCreateInfo.flags:			default
	swapchainCreateInfo.surface				= *canvas.surface;
	swapchainCreateInfo.minImageCount		= m_bufferCount;
	swapchainCreateInfo.imageFormat			= canvas.surfaceFormat;
	swapchainCreateInfo.imageColorSpace		= canvas.surfaceDetails.formats[canvas.surfaceFormatIndex].colorSpace;
	swapchainCreateInfo.imageExtent			= m_extent;
	swapchainCreateInfo.imageArrayLayers	= 1;
	swapchainCreateInfo.imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//swapchainCreateInfo.imageSharingMode:			later
	//swapchainCreateInfo.queueFamilyIndexCount:	later
	//swapchainCreateInfo.pQueueFamilyIndices:		later
	swapchainCreateInfo.preTransform		= caps.currentTransform;
	swapchainCreateInfo.compositeAlpha		= compositeAlpha;
	swapchainCreateInfo.presentMode			= mode;
	swapchainCreateInfo.clipped				= VK_TRUE;
	swapchainCreateInfo.oldSwapchain		= m_handle;

	const std::array<uint32_t, 2> queueFamilies { canvas.getGraphicsQueueFamilyIndex(), canvas.getPresentQueueFamilyIndex() };
	if (canvas.getGraphicsQueueFamilyIndex() != canvas.getPresentQueueFamilyIndex())
	{
		swapchainCreateInfo.imageSharingMode		= VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount	= 2;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 1;
	}
	swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.data();

	destroyFramebuffers();

	VKASSERT2(vkCreateSwapchainKHR(*canvas.device, &swapchainCreateInfo, canvas.callbacks, &m_handle));

	// destroying the old swapchain
	if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
	{
		++m_refreshCount;
		VKASSERT2(vkDeviceWaitIdle(*canvas.device));
		vkDestroySwapchainKHR(*canvas.device, swapchainCreateInfo.oldSwapchain, canvas.callbacks);
	}

	createFramebuffers(rp, m_bufferCount);
	m_renderPass = rp;
}

void Canvas::updateExtent ()
{
	VkSurfaceCapabilitiesKHR caps;
	VKASSERT2(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physicalDevice, *surface, &caps));
	m_width = caps.currentExtent.width;
	m_height = caps.currentExtent.height;
}

Canvas::BackBuffer Canvas::acquireBackBuffer (add_ref<std::queue<BackBuffer>>	buffers,
											 add_ptr<std::mutex>				buffersMutex,
											 add_ref<Swapchain>					swapchain,
											 uint32_t							acquirableImageCount)
{
	std::optional<BackBuffer> buffer;

	if (buffersMutex)
	{
		bool open = false;
		do
		{
			open = buffersMutex->try_lock();
			if (open && buffers.empty())
			{
				buffersMutex->unlock();
				open = false;
				continue;
			}
			else if (open)
			{
				buffer = buffers.front();
				buffers.pop();
				buffersMutex->unlock();
				break;
			}
		}
		while (!open);
	}
	else
	{
		buffer = buffers.front();
		buffers.pop();
	}

	VkResult res = VK_ERROR_UNKNOWN;
	do
	{
		using namespace std::chrono_literals;
		/*
		 * [VL]: Validation Error: [ VUID-vkAcquireNextImageKHR-surface-07783 ]
		 * Object 0: type = VK_OBJECT_TYPE_SWAPCHAIN_KHR; MessageID = 0xad0e15f6 | vkAcquireNextImageKHR:
		 * Application has already previously acquired 1 image from swapchain. Only 1 is available to be acquired
		 * using a timeout of UINT64_MAX (given the swapchain has 3, and VkSurfaceCapabilitiesKHR::minImageCount is 3).
		 * The Vulkan spec states: If forward progress cannot be guaranteed for the surface used to create the swapchain
		 * member of pAcquireInfo, the timeout member of pAcquireInfo must not be UINT64_MAX
		 * (https://vulkan.lunarg.com/doc/view/1.3.243.0/linux/1.3-extensions/vkspec.html#VUID-vkAcquireNextImageKHR-surface-07783)
		 */
		const auto timeout = std::chrono::microseconds( 5s ).count();
		res = vkAcquireNextImageKHR(*device,
									swapchain.handle,
									timeout, *buffer->acquireSemaphore,
									(VkFence)VK_NULL_HANDLE,
									&buffer->imageIndex);
		if (VK_ERROR_OUT_OF_DATE_KHR == res || VK_SUBOPTIMAL_KHR == res)
		{
			if (getGlobalAppFlags().verbose > 9)
			{
				logger << "Recreate swapchain" << std::endl;
			}
			updateExtent();
			swapchain.recreate(swapchain.renderPass, acquirableImageCount, m_width, m_height, true);
		}
		else
		{
			VKASSERT2(res);
		}
	}
	while (VK_SUCCESS != res);

	if (getGlobalAppFlags().verbose > 9)
	{
		logger << "Acquire: " << buffer->imageIndex << ", Width: " << m_width << ", Height: " << m_height << std::endl;
	}

	return *buffer;
}

bool Canvas::presentBackBuffer (add_cref<BackBuffer>				buffer,
								add_ref<Swapchain>					swapchain,
								add_ptr<std::queue<BackBuffer>>		readyBuffersStack,
								add_ptr<std::mutex>					readyBuffersStackMutex,
								add_ptr<std::condition_variable>	readyBufferCondition,
								add_ref<std::queue<BackBuffer>>		readyBuffersQueue,
								const bool							silent)
{
	ZFence		presentFence		= buffer.presentFence;
	ZSemaphore	acquireSemaphore	= buffer.acquireSemaphore;
	ZSemaphore	renderSemaphore		= buffer.renderSemaphore;

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext				= nullptr;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= renderSemaphore.ptr();
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= &swapchain.handle;
	presentInfo.pImageIndices		= &buffer.imageIndex;
	presentInfo.pResults			= nullptr;

	const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			| VK_PIPELINE_STAGE_TRANSFER_BIT;
			//VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo submitInfo{};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount	= 1;
	submitInfo.pWaitSemaphores		= renderSemaphore.ptr();
	submitInfo.pWaitDstStageMask	= &waitDstStageMask;
	submitInfo.signalSemaphoreCount	= 1;
	submitInfo.pSignalSemaphores	= acquireSemaphore.ptr();

	VkResult res = VK_SUCCESS;
	if (silent)
	{
		VKASSERT2(vkQueueSubmit(*graphicsQueue, 1, &submitInfo, (VkFence)VK_NULL_HANDLE));
	}
	else
	{
		res = vkQueuePresentKHR(*presentQueue, &presentInfo);
		VKASSERT2(vkQueueSubmit(*presentQueue, 0, (const VkSubmitInfo*)nullptr, *presentFence));

		waitForFence(presentFence);
		resetFence(presentFence);
	}

	if (readyBuffersStackMutex)
	{
		std::lock_guard<std::mutex> lk(*readyBuffersStackMutex);
		readyBuffersStack->emplace(buffer);
		readyBufferCondition->notify_one();
		UNREF(lk);
	}
	else
	{
		readyBuffersQueue.push(buffer);
	}

	return (VK_SUCCESS == res);
}

void Canvas::render	(add_ref<Swapchain>					swapchain,
					 uint32_t							acquirableImageCount,
					 add_ref<std::queue<BackBuffer>>	buffersQueue,
					 add_ptr<std::mutex>				buffersQueueMutex,
					 add_ptr<std::queue<BackBuffer>>	readyBuffersStack,
					 add_ptr<std::mutex>				readyBuffersStackMutex,
					 add_ptr<std::condition_variable>	readyBufferCondition,
					 OnCommandRecording					onCommandRecording)
{
	BackBuffer	buffer = acquireBackBuffer(buffersQueue, buffersQueueMutex, swapchain, acquirableImageCount);

	VkPipelineStageFlags	singleRenderStageMask;
	VkPipelineStageFlags	multiRenderStageMask;

	if (nullptr == onCommandRecording)
	{
		ZImage presentImage = swapchain.images[buffer.imageIndex];
		if (getGlobalAppFlags().verbose > 9)
		{
			logger << "SourceImage:  " << buffer.blitImage << '\n'
				   << "PresentImage: " << presentImage << '\n'
				   << "Main: " << std::this_thread::get_id() << " Worker: " << buffer.threadID << '\n'
				   << "Render: " << buffer.renderCommand
				   << " Blit: " << buffer.blitCommand << " use_cout " << buffer.blitCommand.use_count() << std::endl;
		}

		ZImageMemoryBarrier barriers[4];
		barriers[0] = makeImageMemoryBarrier(buffer.blitImage,
											 VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
											 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		barriers[1] = makeImageMemoryBarrier(presentImage,
											 VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
											 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		barriers[2] = makeImageMemoryBarrier(buffer.blitImage,
											 VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE,
											 VK_IMAGE_LAYOUT_GENERAL);
		barriers[3] = makeImageMemoryBarrier(presentImage,
											 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
											 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		commandBufferBegin(buffer.blitCommand);
			commandBufferPipelineBarriers(buffer.blitCommand,
										  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										  barriers[0], barriers[1]);
			commandBufferBlitImage(buffer.blitCommand, buffer.blitImage, presentImage);
			commandBufferPipelineBarriers(buffer.blitCommand,
										  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
										  barriers[2], barriers[3]);
		commandBufferEnd(buffer.blitCommand);
		imageResetLayout(presentImage, VK_IMAGE_LAYOUT_UNDEFINED);

		VkSubmitInfo blitSubmitInfo{};

		multiRenderStageMask	= VK_PIPELINE_STAGE_TRANSFER_BIT;

		blitSubmitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		blitSubmitInfo.pNext				= nullptr;
		blitSubmitInfo.commandBufferCount	= 1;
		blitSubmitInfo.pCommandBuffers		= buffer.blitCommand.ptr();
		blitSubmitInfo.waitSemaphoreCount	= 1;
		blitSubmitInfo.pWaitSemaphores		= buffer.acquireSemaphore.ptr();
		blitSubmitInfo.pWaitDstStageMask	= &multiRenderStageMask;
		blitSubmitInfo.signalSemaphoreCount	= 1;
		blitSubmitInfo.pSignalSemaphores	= buffer.renderSemaphore.ptr();

		if (getGlobalAppFlags().verbose > 9)
		{
			logger << "Try multithread mode wait on fence " << buffer.renderFence
					  << " with data created by thread " << buffer.threadID
					  << " and acquired image " << buffer.imageIndex
					  << std::endl;
		}

		ZQueue queue = buffer.blitCommand.getParam<ZCommandPool>().getParam<ZQueue>();
		VKASSERT2(vkQueueSubmit(*queue, 1u, &blitSubmitInfo, *buffer.renderFence));
		VKASSERT2(vkWaitForFences(*device, 1u, buffer.renderFence.ptr(), VK_TRUE, INVALID_UINT64));
		resetFence(buffer.renderFence);
	}
	else
	{
		VkSubmitInfo renderSubmitInfo{};
		singleRenderStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		renderSubmitInfo.sType					= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		renderSubmitInfo.pNext					= nullptr;
		renderSubmitInfo.commandBufferCount		= 1;
		renderSubmitInfo.pCommandBuffers		= buffer.renderCommand.ptr();
		renderSubmitInfo.waitSemaphoreCount		= 1;
		renderSubmitInfo.pWaitSemaphores		= buffer.acquireSemaphore.ptr();
		renderSubmitInfo.pWaitDstStageMask		= &singleRenderStageMask;
		renderSubmitInfo.signalSemaphoreCount	= 1;
		renderSubmitInfo.pSignalSemaphores		= buffer.renderSemaphore.ptr();

		ZFramebuffer framebuffer(swapchain.framebuffers[buffer.imageIndex]);
		onCommandRecording(std::ref(*this), swapchain, buffer.renderCommand, framebuffer);
		imageResetLayout(framebufferGetImage(framebuffer), VK_IMAGE_LAYOUT_UNDEFINED);

		if (getGlobalAppFlags().verbose > 9)
		{
			logger    << "Try single thread mode wait on fence " << buffer.renderFence
					  << " with data created by thread " << buffer.threadID
					  << " and acquired image " << buffer.imageIndex
					  << std::endl;
		}
		ZQueue queue = buffer.renderCommand.getParam<ZCommandPool>().getParam<ZQueue>();
		VKASSERT2(vkQueueSubmit(*queue, 1u, &renderSubmitInfo, *buffer.renderFence));
		VKASSERT2(vkWaitForFences(*device, 1u, buffer.renderFence.ptr(), VK_TRUE, INVALID_UINT64));
		resetFence(buffer.renderFence);
	}

	if (getGlobalAppFlags().verbose > 9)
	{
		logger << "After waiting blitCommand is " << buffer.blitCommand << std::endl;
	}

	presentBackBuffer(buffer, swapchain, readyBuffersStack, readyBuffersStackMutex, readyBufferCondition, buffersQueue, false);
}

void thread_body	(add_ref<std::queue<Canvas::BackBuffer>>	readyBufferStack,
					 add_ref<std::mutex>						readyBufferStackMutex,
					 add_ref<std::condition_variable>			readyBufferCondition,
					 add_ref<std::queue<Canvas::BackBuffer>>	backBufferQueue,
					 add_ref<std::mutex>						backBufferQueueMutex,
					 Canvas::OnSubcommandRecordingThenBlit		performRecord,
					 add_ref<Canvas>							canvas,
					 add_cref<Canvas::Swapchain>				swapchain,
					 add_ref<std::string>						errorString,
					 const uint32_t								threadIndex,
					 add_ref<bool>								doContinue,
					 add_ref<Logger>							logger)
{
	int i = 0;
	if (getGlobalAppFlags().verbose > 9) {
		logger << "thread " << threadIndex << " start" << std::endl; }
	do
	{
		if (getGlobalAppFlags().verbose > 9) {
			logger << "thread " << threadIndex << " continue waiting " << i++ << std::endl; }
		std::unique_lock<std::mutex> lk(readyBufferStackMutex);
		readyBufferCondition.wait(lk, [&]{ return !readyBufferStack.empty() || !doContinue; });
		if (doContinue)
		{
			Canvas::BackBuffer buffer = readyBufferStack.front();
			readyBufferStack.pop();
			if (getGlobalAppFlags().verbose > 9) {
				logger << "thread " << threadIndex << " process data" << std::endl; }
			lk.unlock();
			try
			{
				if (getGlobalAppFlags().verbose > 9) {
					logger << "thread " << threadIndex << " blit command before " << buffer.blitCommand << std::endl; }
				buffer.blitImage = performRecord(canvas, swapchain, buffer.renderCommand, threadIndex);
				buffer.threadID = std::this_thread::get_id();
				commandBufferSubmitAndWait(buffer.renderCommand, buffer.renderFence);
				if (getGlobalAppFlags().verbose > 9) {
					logger << "thread " << threadIndex << " blit command after " << buffer.blitCommand << std::endl; }
				if (getGlobalAppFlags().verbose > 10)
				{
					const VkFormat format = buffer.blitImage.getParamRef<VkImageCreateInfo>().format;
					ZBuffer storage = createBuffer(buffer.blitImage, ZBufferUsageFlags(VK_BUFFER_USAGE_TRANSFER_SRC_BIT), ZMemoryPropertyHostFlags);
					imageCopyToBuffer(buffer.renderCommand.getParam<ZCommandPool>(), buffer.blitImage, storage);
					const ZFormatInfo formatInfo = formatGetInfo(format);
					std::vector<UVec4> d4;
					std::vector<UVec3> d3;
					std::vector<UVec2> d2;
					std::vector<UVec1> d1;
					switch (formatInfo.pixelByteSize) {
					case 4: bufferRead(storage, d4); break;
					case 3: bufferRead(storage, d3); break;
					case 2: bufferRead(storage, d2); break;
					case 1: bufferRead(storage, d1); break;
					}
					logger << "thread " << threadIndex << " blit image data: ";
					for (uint32_t j = 0; j < 5; ++j)
					{
						switch (formatInfo.pixelByteSize) {
						case 4: logger << d4[j] << ' '; break;
						case 3: logger << d3[j] << ' '; break;
						case 2: logger << d2[j] << ' '; break;
						case 1: logger << d1[j] << ' '; break;
						}
					}
					logger << std::endl;
				}
			}
			catch (add_ref<std::runtime_error> error)
			{
				if (getGlobalAppFlags().verbose > 9) {
					logger << "thread " << threadIndex << " runtime error" << std::endl; }
				errorString = error.what();
				doContinue = false;
				readyBufferCondition.notify_all();
				return;
			}

			{
				if (getGlobalAppFlags().verbose > 9) {
					logger << "thread " << threadIndex << " send data to main thread" << std::endl; }
				std::lock_guard<std::mutex> lg(backBufferQueueMutex);
				backBufferQueue.push(buffer);
			}
		}
		else
		{
			if (getGlobalAppFlags().verbose > 9) {
				logger << "thread " << threadIndex << " about to stop" << std::endl; }
		}
	}
	while (doContinue);

	if (getGlobalAppFlags().verbose > 9) {
		logger << "thread " << threadIndex << " exit" << std::endl; }
}

int Canvas::run (OnSubcommandRecordingThenBlit onCommandRecordingThenBlit, add_cref<std::vector<ZQueue>> threadQueues)
{
	ASSERTMSG(threadQueues.size() <= std::thread::hardware_concurrency(),
			  "Queue count exceeds maximum system threads");
	ZCommandPool				blitPool				= createGraphicsCommandPool();
	ASSERTMSG(threadQueues.end() == std::find(threadQueues.begin(), threadQueues.end(), blitPool.getParam<ZQueue>()),
			  "All of thread queues must differ from main graphics queue");
	for (auto qIter = threadQueues.begin(); qIter != threadQueues.end(); ++qIter)
	{
		ASSERTMSG(qIter->has_handle(), "All of thread queues must not be VK_NULL_HANDLE");
		ASSERTMSG(threadQueues.end() == std::find(std::next(qIter), threadQueues.end(), *qIter),
				  "All of thread queues must ne unique");
	}
	Swapchain					swapchain				(*this);
	const uint32_t				acquirableImageCount	= MAX_BACK_BUFFER_COUNT;
	strings						threadErrors			(threadQueues.size());
	bool						doContinue				(true);
	ZRenderPass					renderPass;
	std::queue<BackBuffer>		readyBufferStack;
	std::mutex					readyBufferStackMutex;
	std::condition_variable		readyBufferCondition;
	std::queue<BackBuffer>		backBufferQueue;
	std::mutex					backBufferQueueMutex;
	std::vector<std::thread>	threads;

	swapchain.recreate(renderPass, acquirableImageCount, 0, 0, true);

	for (ZQueue queue : threadQueues)
	{
		readyBufferStack.emplace(createCommandPool(device, queue), blitPool);
	}

	if (style.visible) glfwShowWindow(*cc_window);

	for (uint32_t threadIndex = 0; threadIndex < (uint32_t)(threadQueues.size()); ++threadIndex)
	{
		threads.emplace_back(thread_body,
					std::ref(readyBufferStack),
					std::ref(readyBufferStackMutex),
					std::ref(readyBufferCondition),
					std::ref(backBufferQueue),
					std::ref(backBufferQueueMutex),
					onCommandRecordingThenBlit,
					std::ref(*this),
					std::cref(swapchain),
					std::ref(threadErrors.at(threadIndex)),
					threadIndex,
					std::ref(doContinue),
					std::ref(logger));
	}

	while (!glfwWindowShouldClose(*window) && doContinue)
	{
		glfwPollEvents();

		render	(swapchain,
				 acquirableImageCount,
				 backBufferQueue,
				 &backBufferQueueMutex,
				 &readyBufferStack,
				 &readyBufferStackMutex,
				 &readyBufferCondition,
				 nullptr);
	}

	doContinue = false;
	readyBufferCondition.notify_all();

	for (add_ref<std::thread> thread : threads)
	{
		thread.join();
	}

	for (add_cref<std::string> errorString : threadErrors)
	{
		if (!errorString.empty()) logger << "ERROR: " << errorString << std::endl;
	}

	return std::all_of(threadErrors.begin(), threadErrors.end(),
					   [](add_cref<std::string> e) { return e.empty(); })
		? 0
		: 1;
}

int Canvas::run (OnCommandRecording				onCommandRecording,
				 ZRenderPass					renderPass,
				 std::reference_wrapper<int>	drawTrigger,
				 OnIdle							onIdle)
{
	Swapchain				swapchain				(*this);
	const uint32_t			acquirableImageCount	= MAX_BACK_BUFFER_COUNT;
	ZCommandPool			graphicsPool			= createGraphicsCommandPool();
	std::queue<BackBuffer>	backBuffersQueue;

	swapchain.recreate(renderPass, acquirableImageCount, 0, 0, true);
	for (uint32_t i = 0; i < acquirableImageCount; ++i)
	{
		backBuffersQueue.emplace(graphicsPool, graphicsPool);
	}

	if (style.visible) glfwShowWindow(*cc_window);

	while (!glfwWindowShouldClose(*window))
	{
		glfwPollEvents();

		if (&m_drawTrigger != &drawTrigger.get())
		{
			if (onIdle)
			{
				onIdle(*this, drawTrigger);
			}

			if (drawTrigger <= 0)
			{
				std::this_thread::yield();
				continue;
			}
			--drawTrigger;
		}

		render	(swapchain,
				 acquirableImageCount,
				 backBuffersQueue,
				 nullptr, // buffersQueueMutex
				 nullptr, // eadyBuffersStack
				 nullptr, // readyBuffersStackMutex
				 nullptr, // readyBufferCondition
				 onCommandRecording);
	}

	return 0;
}

} // namespace vtf
