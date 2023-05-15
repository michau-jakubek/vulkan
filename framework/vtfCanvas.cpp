#include <array>
#include <functional>
#include <thread>
#include <chrono>
#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"
#include "vtfCUtils.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"

#define MAX_BACK_BUFFER_COUNT 2

namespace vtf
{

Canvas::~Canvas	()
{
	while (!m_backBuffers.empty())
	{
		BackBuffer& buf = m_backBuffers.front();
		buf.destroy(*this);
		m_backBuffers.pop_front();
	}
}

int Canvas::m_drawTrigger;
const CanvasStyle Canvas::DefaultStyle { 800u, 600u, 0.0f, 100.0f, true, true };
extern std::map<VkQueueFlagBits, uint32_t> globalQueuesToIndices;
uint32_t globalPresentQueueFamilyIndex;

strings			getGlfwRequiredInstanceExtensions ();
ZGLFWwindowPtr	createWindow (const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer);
ZSurfaceKHR		createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window);

CanvasContext::CanvasContext (const char*			appName,
							  uint32_t				apiVersion,
							  const strings&		instanceLayers,
							  const strings&		instanceExtensions,
							  const strings&		deviceExtensions,
							  GetEnabledFeaturesCB	onGetEnabledFeatures,
							  bool					enableDebugPrintf,
							  const CanvasStyle&	style,
							  add_ptr<Canvas>		canvas)
	: cc_callbacks		(getAllocationCallbacks())
	, cc_debugMessenger	(VK_NULL_HANDLE)
	, cc_debugReport	(VK_NULL_HANDLE)
	, cc_instance		(createInstance(appName, cc_callbacks, instanceLayers, mergeStringsDistinct(getGlfwRequiredInstanceExtensions(), instanceExtensions),
										&cc_debugMessenger, this, &cc_debugReport, this, apiVersion, enableDebugPrintf))
	, cc_window			(createWindow(style, appName, canvas))
	, cc_surface		(createSurface(cc_instance, cc_callbacks, cc_window))
	, cc_physicalDevice	(selectPhysicalDevice(VulkanContext::deviceIndex, cc_instance, deviceExtensions, globalQueuesToIndices, cc_surface, &globalPresentQueueFamilyIndex))
	, cc_device			(createLogicalDevice(cc_physicalDevice, globalQueuesToIndices, onGetEnabledFeatures, globalPresentQueueFamilyIndex))
{
}

Canvas::Canvas	(const char*			appName,
				 const strings&			instanceLayers,
				 const strings&			instanceExtensions,
				 const strings&			deviceExtensions,
				 const CanvasStyle&		style,
				 GetEnabledFeaturesCB	onGetEnabledFeatures,
				 bool					enableDebugPrintf,
				 uint32_t				apiVersion)
	: GlfwInitializerFinalizer()
	, CanvasContext(appName, apiVersion, instanceLayers, instanceExtensions, deviceExtensions, onGetEnabledFeatures, enableDebugPrintf, style, this)
	, VulkanContext	(cc_callbacks, cc_debugMessenger, cc_debugReport, cc_instance, cc_physicalDevice, cc_device)
	// beginning references initialization
	, window					(cc_window)
	, surface					(cc_surface)
	, surfaceDetails			(m_surfaceDetails)
	, format					(m_format)
	, swapchain					(m_swapChain)
	, width						(m_width)
	, height					(m_height)
	, style						(m_style)
	// TODO , drawTrigger				(m_drawTrigger)
	// end of references initialization
	, m_surfaceDetails			()
	, m_format					()
	, m_width					(style.width)
	, m_height					(style.height)
	, m_style					(style)
	, m_backBuffers				()
	, m_commandFences			()
	, m_swapChain				(*this)
	, m_events					(new GLFWEvents(*this))
	, m_timerCallback			()
	, m_timerUserData			(nullptr)
	, m_timerPeriodMS			(0)
{
	for (uint32_t i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
	{
		m_backBuffers.push_back(BackBuffer(*this));
		m_commandFences.emplace_back(*this);
	}

	m_surfaceDetails.update(*physicalDevice, *surface);
	m_format = m_surfaceDetails.formats[0];
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
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// invisible at start, if style.visible is set the call glfwShowWindows() later
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, (style.resizable ? GLFW_TRUE : GLFW_FALSE));
	GLFWwindow* pWindow = glfwCreateWindow(static_cast<int>(style.width), static_cast<int>(style.height), (title ? title : "Vulkan"), nullptr, nullptr);
	ZGLFWwindowPtr window = ZGLFWwindowPtr::create(pWindow);
	ASSERTMSG(*window, "Failed to create GLFW window");
	glfwSetWindowUserPointer(*window, windowUserPointer);

	return window;
}

ZSurfaceKHR	createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window)
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VKASSERT2(glfwCreateWindowSurface(*instance, *window, callbacks, &surface));
	return ZSurfaceKHR::create(surface, instance, callbacks, window);
}

Canvas::SurfaceDetails::SurfaceDetails ()
	: caps		()
	, formats	()
	, modes		()
{
}

void Canvas::SurfaceDetails::update (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR)
{
	VKASSERT2(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surfaceKHR, &caps));

	uint32_t formatCount = 0;;
	VKASSERT2(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surfaceKHR, &formatCount, nullptr));
	ASSERTION(formatCount != 0);
	formats.resize(formatCount);
	VKASSERT2(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surfaceKHR, &formatCount, formats.data()));

	uint32_t presentModeCount = 0;
	VKASSERT2(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surfaceKHR, &presentModeCount, nullptr));
	ASSERTION(presentModeCount != 0);
	modes.resize(presentModeCount);
	VKASSERT2(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surfaceKHR, &presentModeCount, modes.data()));
}

void Canvas::BackBuffer::destroy (VulkanContext& ctx)
{
	UNREF(ctx);
}

Canvas::BackBuffer::BackBuffer (VulkanContext& ctx)
	: imageIndex		(0)
	, acquireSemaphore	(ctx.createSemaphore())
	, renderSemaphore	(ctx.createSemaphore())
	, presentFence		(ctx.createFence(true))
{
	acquireSemaphore.use_count();
}

Canvas::BackBuffer::BackBuffer (const BackBuffer& src)
	: imageIndex		(src.imageIndex)
	, acquireSemaphore	(src.acquireSemaphore)
	, renderSemaphore	(src.renderSemaphore)
	, presentFence		(src.presentFence)
{
}

ZQueue Canvas::getPresentQueue () const
{
	add_cref<ZQueueInfos> queueInfos = device.getParamRef<ZQueueInfos>();
	return *queueInfos.at(ZQueueType::Present);
}

uint32_t Canvas::getPresentQueueFamilyIndex () const
{
	return getPresentQueue().getParam<uint32_t>();
}

Canvas::SwapChain::Framebuffer::Framebuffer ()
	: image			()
	, view			()
	, depth			()
	, framebuffer	()
{
}

Canvas::SwapChain::SwapChain (Canvas& canvas)
	: handle		(m_handle)
	, viewport		(m_viewport)
	, scissor		(m_scissor)
	, extent		(m_extent)
	, buffers		(m_buffers)
	, bufferCount	(m_bufferCount)
	, refreshCount	(m_refreshCount)
	, m_canvas		(canvas)
	, m_handle		(VK_NULL_HANDLE)
	, m_buffers		()
	, m_refreshCount()
	, m_viewport	()
	, m_scissor		()
	, m_extent		()
	, m_bufferCount	()
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

Canvas::SwapChain::~SwapChain ()
{
	vkDeviceWaitIdle(*m_canvas.device);

	destroy(true);

	if (VK_NULL_HANDLE != m_handle)
	{
		vkDestroySwapchainKHR(*m_canvas.device, m_handle, m_canvas.callbacks);
		m_handle = VK_NULL_HANDLE;
	}
}

void Canvas::SwapChain::destroy (bool clear)
{
	UNREF(clear);
	for (auto& buf : m_buffers)
	{
		if (buf.framebuffer != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(*m_canvas.device, buf.framebuffer, m_canvas.callbacks);
			buf.framebuffer = VK_NULL_HANDLE;
		}
		if (buf.view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(*m_canvas.device, buf.view, m_canvas.callbacks);
			buf.view = VK_NULL_HANDLE;
		}
		if (buf.depth.has_value())
		{
			buf.depth.reset();
		}
	}
}

void Canvas::SwapChain::rebuild (ZRenderPass rp)
{
	ASSERTION(m_handle != VK_NULL_HANDLE);

	ZDevice						localDevice		= rp.getParam<ZDevice>();
	VkAllocationCallbacksPtr	localCallbacks	= localDevice.getParam<VkAllocationCallbacksPtr>();

	std::vector<VkImage>		images;
	const uint32_t				imageCount = enumerateSwapchainImages(*localDevice, m_handle, images);
	ASSERTION(imageCount >= bufferCount);

	destroy(false);

	m_buffers.resize(bufferCount);
	const bool hasDepthBuffer = rp.getParam<bool>();
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		Framebuffer& fb = m_buffers[i];

		fb.image		= images[i];

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType		= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image		= images[i];
		viewInfo.viewType	= VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format		= m_canvas.format.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		VKASSERT2(vkCreateImageView(*localDevice, &viewInfo, localCallbacks, &fb.view));

		if (hasDepthBuffer)
		{
			const VkFormat depthFormat	= rp.getParam<VkFormat>();
			ZImage img = ::vtf::createImage(localDevice, depthFormat, m_extent.width, m_extent.height, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
			fb.depth = ::vtf::createImageView(img, VK_IMAGE_ASPECT_DEPTH_BIT);
		}

		VkImageView	attachments[2] { fb.view, (hasDepthBuffer ? **fb.depth : VK_NULL_HANDLE) };

		VkFramebufferCreateInfo fbInfo{};
		fbInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass		= *rp;
		fbInfo.attachmentCount	= hasDepthBuffer ? 2 : 1;
		fbInfo.pAttachments		= attachments;
		fbInfo.width			= m_extent.width;
		fbInfo.height			= m_extent.height;
		fbInfo.layers			= 1;

		VKASSERT2(vkCreateFramebuffer(*localDevice, &fbInfo, localCallbacks, &fb.framebuffer));
	}
}

void Canvas::SwapChain::setup (ZRenderPass rp, uint32_t hintWidth, uint32_t hintHeight, bool force)
{
	VkSurfaceCapabilitiesKHR caps;
	VKASSERT2(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*m_canvas.physicalDevice, *m_canvas.surface, &caps));

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
	m_viewport.minDepth	= m_canvas.style.minDepth;
	m_viewport.maxDepth	= m_canvas.style.maxDepth;

	m_scissor.offset	= {0, 0};
	m_scissor.extent	= caps.currentExtent;

	m_extent			= caps.currentExtent;

	// avoid caps.maxImageCount is 0, it means no limit
	caps.maxImageCount	= std::max(caps.maxImageCount, (uint32_t)MAX_BACK_BUFFER_COUNT);
	m_bufferCount		= std::min(caps.maxImageCount, (uint32_t)MAX_BACK_BUFFER_COUNT);

	ASSERTION(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	ASSERTION(caps.supportedTransforms & caps.currentTransform);
	ASSERTION(caps.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR));
	const VkCompositeAlphaFlagBitsKHR compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
														? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
														: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// FIFO is the only mode universally supported
	const VkPresentModeKHR mode = select_if(m_canvas.surfaceDetails.modes, VK_PRESENT_MODE_FIFO_KHR,
											[](const auto& m) { return (m == VK_PRESENT_MODE_MAILBOX_KHR || m == VK_PRESENT_MODE_IMMEDIATE_KHR); });

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType				= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface				= *m_canvas.surface;
	swapchainCreateInfo.minImageCount		= m_bufferCount;
	swapchainCreateInfo.imageFormat			= m_canvas.format.format;
	swapchainCreateInfo.imageColorSpace		= m_canvas.format.colorSpace;
	swapchainCreateInfo.imageExtent			= caps.currentExtent;
	swapchainCreateInfo.imageArrayLayers	= 1;
	swapchainCreateInfo.imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchainCreateInfo.preTransform		= caps.currentTransform;
	swapchainCreateInfo.compositeAlpha		= compositeAlpha;
	swapchainCreateInfo.presentMode			= mode;
	swapchainCreateInfo.clipped				= true;
	swapchainCreateInfo.oldSwapchain		= m_handle;

	const std::array<uint32_t, 2> queueFamilies { m_canvas.getGraphicsQueueFamilyIndex(), m_canvas.getPresentQueueFamilyIndex() };
	if (m_canvas.getGraphicsQueueFamilyIndex() !=m_canvas.getPresentQueueFamilyIndex())
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

	VKASSERT2(vkCreateSwapchainKHR(*m_canvas.device, &swapchainCreateInfo, m_canvas.callbacks, &m_handle));

	rebuild(rp);

	// destroy the old swapchain
	if (swapchainCreateInfo.oldSwapchain != VK_NULL_HANDLE)
	{
		++m_refreshCount;
		VKASSERT2(vkDeviceWaitIdle(*m_canvas.device));
		vkDestroySwapchainKHR(*m_canvas.device, swapchainCreateInfo.oldSwapchain, m_canvas.callbacks);
	}
}

Canvas::CommandAndFence::CommandAndFence (Canvas& c)
	: fence		(c.createFence(true))
	, cmdBuffer	(c.createCommandBuffer(c.createGraphicsCommandPool()))
{
}

Canvas::CommandAndFence::CommandAndFence (ZCommandPool pool)
	: fence		(::vtf::createFence(pool.getParam<ZDevice>(), false))
	, cmdBuffer	(::vtf::createCommandBuffer(pool))
{
}

void Canvas::setTimer (Canvas::TimerCallback callback, void* userData, uint64_t milliseconds)
{
	m_timerCallback = callback;
	m_timerUserData	= userData;
	m_timerPeriodMS	= milliseconds;
}

VkImage Canvas::getSwapchainImage (uint32_t swapImageIndex) const
{
	return m_swapChain.buffers.at(swapImageIndex).image;
}

VkRenderPassBeginInfo Canvas::makeRenderPassBeginInfo (ZRenderPass rp, uint32_t swapImageIndex) const
{
	auto&					clearColors		= rp.getParamRef<std::vector<VkClearValue>>();
	const VkClearValue*		pClearValues	= clearColors.size() ? clearColors.data() : nullptr;
	VkRenderPassBeginInfo	renderPassInfo{};

	renderPassInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass			= *rp;
	renderPassInfo.framebuffer			= m_swapChain.buffers.at(swapImageIndex).framebuffer;
	ASSERTION(VK_NULL_HANDLE != renderPassInfo.framebuffer);
	renderPassInfo.renderArea.offset	= {0, 0};
	renderPassInfo.renderArea.extent	= m_swapChain.extent;
	renderPassInfo.clearValueCount		= static_cast<uint32_t>(clearColors.size());
	renderPassInfo.pClearValues			= pClearValues;

	return renderPassInfo;
}

void Canvas::transitionImageForPresent (ZCommandBuffer cmdBuffer, uint32_t swapImageIndex, VkImageLayout oldLayout)
{
	VkPipelineStageFlags	stage			= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkImageMemoryBarrier	transitionBarrier{};
	transitionBarrier.sType					= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	transitionBarrier.pNext					= nullptr;
	transitionBarrier.srcAccessMask			= VK_ACCESS_MEMORY_READ_BIT;
	transitionBarrier.dstAccessMask			= VK_ACCESS_MEMORY_WRITE_BIT;
	transitionBarrier.oldLayout				= oldLayout;
	transitionBarrier.newLayout				= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	transitionBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	transitionBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	transitionBarrier.image					= m_swapChain.buffers.at(swapImageIndex).image;
	transitionBarrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	transitionBarrier.subresourceRange.baseMipLevel		= 0;
	transitionBarrier.subresourceRange.levelCount		= 1;
	transitionBarrier.subresourceRange.baseArrayLayer	= 0;
	transitionBarrier.subresourceRange.layerCount		= 1;

	vkCmdPipelineBarrier(*cmdBuffer, stage, stage, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &transitionBarrier);
}

void Canvas::releaseBackBuffers ()
{
	std::vector<ZFence>	fences;
	std::transform(m_backBuffers.begin(), m_backBuffers.end(), std::back_inserter(fences), [](const auto& buf) { return buf.presentFence; });
	waitForFences(fences);
	resetFences(fences);
}

Canvas::BackBuffer Canvas::acquireBackBuffer (ZRenderPass rp)
{
	auto& buffer = m_backBuffers.front();

	ZFence		presentFence		= buffer.presentFence;
	ZSemaphore	acquireSemaphore	= buffer.acquireSemaphore;

	waitForFence(presentFence);
	resetFence(presentFence);

	VkResult res = VK_INCOMPLETE;
	do
	{
		res = vkAcquireNextImageKHR(*device, m_swapChain.handle, UINT64_MAX, *acquireSemaphore, (VkFence)VK_NULL_HANDLE, &buffer.imageIndex);
		if (VK_ERROR_OUT_OF_DATE_KHR == res || VK_SUBOPTIMAL_KHR == res)
		{
			m_swapChain.setup(rp);
			m_width	= m_swapChain.extent.width;
			m_height = m_swapChain.extent.height;
		}
		else
		{
			VKASSERT2(res);
		}
	} while (VK_SUCCESS != res);

	Canvas::BackBuffer result(buffer);
	m_backBuffers.pop_front();

	return result;
}

void Canvas::presentBackBuffer (ZRenderPass rp, const Canvas::BackBuffer& buffer)
{
	ZFence		presentFence	= buffer.presentFence;
	ZSemaphore	renderSemaphore = buffer.renderSemaphore;

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext				= nullptr;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= renderSemaphore.ptr();
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= &m_swapChain.handle;
	presentInfo.pImageIndices		= &buffer.imageIndex;
	presentInfo.pResults			= nullptr;

	const VkResult res = vkQueuePresentKHR(*getPresentQueue(), &presentInfo);
	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapChain.setup(rp);
		m_width	= m_swapChain.extent.width;
		m_height = m_swapChain.extent.height;
	}
	else
	{
		ASSERTION(res == VK_SUCCESS);
	}

	VKASSERT2(vkQueueSubmit(*getPresentQueue(), 0, (const VkSubmitInfo*)nullptr, *presentFence));

	m_backBuffers.push_back(buffer);
}

GLFWEvents& Canvas::events()
{
	return *m_events;
}

int Canvas::run (ZRenderPass rp, OnCommandRecordingCallback onCommandRecording, std::reference_wrapper<int> drawTrigger)
{
	ASSERTION(onCommandRecording != nullptr);
	if (&m_drawTrigger != &drawTrigger.get())
	{
		ASSERTMSG((drawTrigger > 0), "Plase call onCommandRecording at least once");
	}

	m_swapChain.setup(rp, m_width, m_height, true);

	bool							doContinue			= true;
	uint32_t						commandBufferIndex	= 0;
	std::vector<CommandAndFence>	commandBuffers		{};
	const uint32_t					backBufferCount		= m_swapChain.bufferCount;
	ZCommandPool					commandPool			= createGraphicsCommandPool();
	for (uint32_t i = 0; i < backBufferCount; ++i)	{ commandBuffers.emplace_back(commandPool); }

	if (style.visible) glfwShowWindow(*cc_window);

	while (doContinue && !glfwWindowShouldClose(*cc_window))
	{
		glfwPollEvents();

		if (&m_drawTrigger != &drawTrigger.get())
		{
			if (drawTrigger <= 0)
			{
				std::this_thread::yield();
				continue;
			}
			--drawTrigger;
		}

		for (uint32_t i = 0; doContinue && i < backBufferCount; ++i)
		{
			auto buf = acquireBackBuffer(rp);
			{
				ZSemaphore				renderSemaphore		= buf.renderSemaphore;
				ZSemaphore				acquireSemaphore	= buf.acquireSemaphore;
				ZCommandBuffer			drawCommand			= commandBuffers.at(commandBufferIndex).cmdBuffer;
				ZFence					fence				= commandBuffers.at(commandBufferIndex).fence;

				doContinue = onCommandRecording(*this, drawCommand, buf.imageIndex);

				VkSubmitInfo	submitInfo{};
				submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.pNext				= nullptr;
				submitInfo.commandBufferCount	= 1;
				submitInfo.pCommandBuffers		= drawCommand.ptr();
				submitInfo.waitSemaphoreCount	= 1;
				submitInfo.pWaitSemaphores		= acquireSemaphore.ptr();
				submitInfo.pWaitDstStageMask	= &static_cast<const VkPipelineStageFlags&>(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
				submitInfo.signalSemaphoreCount	= 1;
				submitInfo.pSignalSemaphores	= renderSemaphore.ptr();

				VKASSERT2(vkQueueSubmit(*getGraphicsQueue(), 1, &submitInfo, *fence));

				waitForFence(fence);
				resetFence(fence);

				commandBufferIndex = (commandBufferIndex + 1) % backBufferCount;
			}
			presentBackBuffer(rp, buf);
		}
	}

	releaseBackBuffers();

	return 0;
}

} // namespace vtf
