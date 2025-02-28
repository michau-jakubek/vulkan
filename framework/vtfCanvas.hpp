#ifndef __VTF_CANVAS_HPP_INCLUDED__
#define __VTF_CANVAS_HPP_INCLUDED__

#include "GLFW/glfw3.h"
#include "vtfVkUtils.hpp"
#include "vtfContext.hpp"
#include "vtfZImage.hpp"
#include "vtfThreadSafeLogger.hpp"
#include "vtfTemplateUtils.hpp"

#include <condition_variable>
#include <thread>
#include <mutex>
#include <queue>

namespace vtf
{

struct GLFWEvents;
class Canvas;

struct GlfwInitializerFinalizer
{
	bool m_initialized;
	GlfwInitializerFinalizer (ZInstance instance, bool initialize = true);
	virtual ~GlfwInitializerFinalizer ();
	void init(ZInstance instance);
};

struct CanvasStyle
{
	uint32_t				width;
	uint32_t				height;
	float					minDepth;
	float					maxDepth;
	bool					visible;
	bool					resizable;
	VkFormatFeatureFlags	surfaceFormatFlags;
};

struct CanvasContext
{
	VkAllocationCallbacksPtr	cc_callbacks;
	ZInstance					cc_instance;
	GlfwInitializerFinalizer	cc_glfw;
	ZGLFWwindowPtr				cc_window;
	ZSurfaceKHR					cc_surface;
	ZPhysicalDevice				cc_physicalDevice;
	ZDevice						cc_device;
	CanvasContext	(add_cptr<char>			appName,
					 add_cref<Version>		apiVersion,
					 add_cref<strings>		instanceLayers,
					 add_cref<strings>		instanceExtensions,
					 add_cref<strings>		deviceExtensions,
					 OnEnablingFeatures		onEnablingFeatures,
					 bool					enableDebugPrintf,
					 add_cref<CanvasStyle>	style,
					 add_ptr<Canvas>		canvas);
	CanvasContext	(ZPhysicalDevice		physicalDevice,
					 OnEnablingFeatures		onEnablingFeatures,
					 bool					enableDebugPrintf,
					 add_cref<CanvasStyle>	style,
					 add_ptr<Canvas>		canvas);
	virtual ~CanvasContext();
	void closeWindow ();
};

class Canvas : public CanvasContext, public VulkanContext
{
public:
	struct SurfaceDetails
	{
		VkSurfaceCapabilitiesKHR		caps;
		std::vector<VkSurfaceFormatKHR>	formats;
		std::vector<VkPresentModeKHR>	modes;
		SurfaceDetails ();
		void update (ZPhysicalDevice physDevice, ZSurfaceKHR surfaceKHR);
		uint32_t selectFormat (ZPhysicalDevice device, VkFormatFeatureFlags formatFlags);
	};

	class Swapchain
	{
	public:
		typedef std::vector<ZFramebuffer> Framebuffers;
		typedef std::vector<ZNonDeletableImage> Images;
		friend class Canvas;
		Swapchain (add_cref<Canvas>);
		Swapchain (Swapchain&&) = delete;
		~Swapchain ();
		add_cref<Canvas>			canvas;
		add_cref<VkSwapchainKHR>	handle;
		add_cref<VkViewport>		viewport;
		add_cref<VkRect2D>			scissor;
		add_cref<VkExtent2D>		extent;
		add_cref<Images>			images;
		add_cref<Framebuffers>		framebuffers;
		add_cref<uint32_t>			bufferCount;
		add_cref<uint32_t>			refreshCount;
		add_cref<ZRenderPass>		renderPass;	// accessible after recreation
		void recreate (ZRenderPass rp, uint32_t acquirableImageCount, uint32_t hintWidth = 0, uint32_t hintHeight = 0, bool force = false);
		add_cref<bool> recreateFlag;
	private:
		// If rp has no handle then creates only images, otherwise views and framebuffers as well
		void createFramebuffers (ZRenderPass rp, uint32_t minImageCount);
		void destroyFramebuffers ();
		void resetRecreateFlag () { m_recreateFlag = false; }
		VkSwapchainKHR				m_handle;
		Images						m_images;
		Framebuffers				m_framebuffers;
		uint32_t					m_refreshCount;
		VkViewport					m_viewport;
		VkRect2D					m_scissor;
		VkExtent2D					m_extent;
		uint32_t					m_bufferCount;
		ZRenderPass					m_renderPass;
		bool						m_recreateFlag;
	};

	struct BackBuffer
	{
		ZDevice			device;
		uint32_t		imageIndex;
		ZImage			blitImage;
		ZCommandBuffer	blitCommand;
		ZFence			blitFence;
		ZSemaphore		acquireSemaphore;
		ZSemaphore		renderSemaphore;
		ZFence			presentFence;
		ZCommandBuffer	renderCommand;
		ZFence			renderFence;
		uint32_t		threadIndex;
		std::thread::id	threadID;

		BackBuffer (ZCommandPool renderPool, ZCommandPool blitPool);
		BackBuffer (add_cref<BackBuffer>);
		add_ref<BackBuffer> operator=(add_cref<BackBuffer>);
		void recreateAcquireSemaphore ();
	};

	template<class Float> struct Area
	{
		Float xMin;
		Float xMax;
		Float yMin;
		Float yMax;
		void scale (Float);
		void move (Float, Float);
		Float width () const;
		Float height () const;
		Area () : xMin(), xMax(), yMin(), yMax() {}
		Area (Float XMin, Float XMax, Float YMin, Float YMax)
			: xMin(XMin), xMax(XMax), yMin(YMin), yMax(YMax) {}
		static auto userArea() { return Area<float>(-1.0f, +1.0f, -1.0f, +1.0f); }
		template<class W> auto windowArea(add_cref<Canvas> canvas)
		{
			return Area<W>(W(0), W(canvas.width), W(0), W(canvas.height));
		}
	};

	static const CanvasStyle DefaultStyle; // 800,600,0,1,true,true

public:
	// By default all device features are disabled so if you want to enable any of them, these must be
	// known before logical device is created. To express the features you can use OnEnablingFeatures
	// callback where both physicalDevice and a reference to extension list are given as parameters.
	// If present then prepare deviceExtensions you want and return your VkPhysicalDeviceFeatures2 struct
	// with desired features enabled  from onEnablingFeatures callback. This structure will be passed as
	// VkDeviceCreateInfo::pNext during logical device creation. Do not care about sType field and pNext
	// field of that struct, these will be populated properly behind the scene. There is only the one
	// drawback because all fetures have to be declared outside this callback to avoid segmentation fault.
	// Extension list is a full list of the extensions achieved from physical device so basically you
	// might need to remove particular extension string instead of add new one.
	// Typically a signature and a body of that callback would look like this:
	// auto onEnablingFeatures = [](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	// {
	// };
	Canvas	(add_cptr<char>			appName,
			 add_cref<strings>		instanceLayers			= {},
			 add_cref<strings>		instanceExtensions		= {},
			 add_cref<strings>		deviceExtensions		= {},
			 add_cref<CanvasStyle>	canvasStyle				= DefaultStyle,
			 OnEnablingFeatures		onEnablingFeatures		= nullptr,
			 add_cref<ApiVersion>	apiVersion				= Version(1,0),
			 bool					enableDebugPrintf		= false);

	Canvas	(ZPhysicalDevice		physicalDevice,
			 add_cref<CanvasStyle>	canvasStyle			= DefaultStyle,
			 OnEnablingFeatures		onEnablingFeatures	= nullptr,
			 bool					enableDebugPrintf	= false);

	virtual ~Canvas	();

	add_cref<ZGLFWwindowPtr>	window;
	add_cref<ZSurfaceKHR>		surface;
	add_cref<SurfaceDetails>	surfaceDetails;
	add_cref<uint32_t>			surfaceFormatIndex;
	add_cref<VkFormat>			surfaceFormat;
	const CanvasStyle			style;
	add_cref<uint32_t>			width;
	add_cref<uint32_t>			height;
	add_cref<ZQueue>			presentQueue;
	uint32_t					getPresentQueueFamilyIndex () const;
	inline add_ref<GLFWEvents>	events () { return *m_events; }

	void updateExtent();

	typedef std::function<void (add_ref<Canvas>)> OnAfterRecording;
	typedef std::function<void (add_ref<Canvas>, add_ref<int> drawTrigger)> OnIdle;
	typedef std::function<void (add_ref<Canvas>, add_cref<Swapchain>, ZCommandBuffer, ZFramebuffer)> OnCommandRecording;
	typedef std::function<ZImage (add_ref<Canvas>, add_cref<Swapchain>, ZCommandBuffer, uint32_t threadID)> OnSubcommandRecordingThenBlit;
	int						run					(OnCommandRecording				onCommandRecording,
												 ZRenderPass					renderPass,
												 std::reference_wrapper<int>	drawTrigger,
												 OnIdle							onIdle = {},
												 OnAfterRecording				onAfterRecording = {});
	int						run					(OnSubcommandRecordingThenBlit	onCommandRecordingThenBlit,
												 add_cref<std::vector<ZQueue>>	threadQueues);

	template<class U, class W = U>
	void	userToWindow		(const Area<U>& userArea, const VecX<U,2>& userPoint, VecX<W,2>& windowPoint,
															bool invertY = false) const;
	template<class W, class U = W>
	void	windowToUser		(const Area<U>& userArea, const VecX<W,2>& windowPoint, VecX<U,2>& userPoint,
															bool invertY = false) const;

protected:

	BackBuffer			acquireBackBuffer		(add_ref<std::queue<BackBuffer>>	buffers,
												 add_ptr<std::mutex>				buffersMutex,
												 add_ref<Swapchain>					swapchain,
												 uint32_t							acquirableImageCount);
	bool				presentBackBuffer		(add_cref<BackBuffer>				buffer,
												 add_ref<Swapchain>					swapchain,
												 add_ptr<std::queue<BackBuffer>>	readyBuffersStack,
												 add_ptr<std::mutex>				readyBuffersStackMutex,
												 add_ptr<std::condition_variable>	readyBufferCondition,
												 add_ref<std::queue<BackBuffer>>	readyBuffersQueue,
												 const bool							silent);
	void				render					(add_ref<Swapchain>					swapchain,
												 uint32_t							acquirableImageCount,
												 add_ref<std::queue<BackBuffer>>	buffersQueue,
												 add_ptr<std::mutex>				buffersQueueMutex,
												 add_ptr<std::queue<BackBuffer>>	readyBuffersStack,
												 add_ptr<std::mutex>				readyBuffersStackMutex,
												 add_ptr<std::condition_variable>	readyBufferCondition,
												 OnCommandRecording					onCommandRecording);
	void				construct				();

	friend struct GLFWEvents;

	SurfaceDetails					m_surfaceDetails;
	uint32_t						m_surfaceFormatIndex;
	VkFormat						m_surfaceFormat;
	uint32_t						m_width;
	uint32_t						m_height;
	ZQueue							m_presentQueue;
	std::unique_ptr<GLFWEvents>		m_events;
	void*							m_timerUserData;
	uint64_t						m_timerPeriodMS;
	static int						m_drawTrigger;
};

template<class UType, class WType>
void Canvas::userToWindow (
	const Canvas::Area<UType>&	userArea,
	const VecX<UType,2>&		userPoint,
	VecX<WType,2>&				windowPoint,
	bool						invertY) const
{
	transformDistance(userArea.xMin, userArea.xMax, userPoint.x(), WType(0u), WType(width), windowPoint.x(), false);
	transformDistance(userArea.yMin, userArea.yMax, userPoint.y(), WType(0u), WType(height), windowPoint.y(), invertY);
}

template<class WType, class UType>
void Canvas::windowToUser (
	const Canvas::Area<UType>&	userArea,
	const VecX<WType,2>&		windowPoint,
	VecX<UType,2>&				userPoint,
	bool						invertY) const
{
	transformDistance(WType(0u), WType(width), windowPoint.x(), userArea.xMin, userArea.xMax, userPoint.x(), false);
	transformDistance(WType(0u), WType(height), windowPoint.y(), userArea.yMin, userArea.yMax, userPoint.y(), invertY);
}

template<class Float> Float Canvas::Area<Float>::width () const { return std::abs(xMax - xMin); }
template<class Float> Float Canvas::Area<Float>::height () const { return std::abs(yMax - yMin); }
template<class Float> void Canvas::Area<Float>::scale (Float f)
{
	xMin *= f;
	xMax *= f;
	yMin *= f;
	yMax *= f;
}
template<class Float> void Canvas::Area<Float>::move (Float dx, Float dy)
{
	xMin += dx;
	xMax += dx;
	yMin += dy;
	yMax += dy;
}

} // namespace vtf

#endif // __VTF_CANVAS_HPP_INCLUDED__
