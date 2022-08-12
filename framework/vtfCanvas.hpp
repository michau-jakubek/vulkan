#ifndef __VTF_CANVAS_HPP_INCLUDED__
#define __VTF_CANVAS_HPP_INCLUDED__

#include <deque>
#include <optional>
#include "GLFW/glfw3.h"
#include "vtfContext.hpp"

namespace vtf
{

struct GLFWEvents;
class Canvas;

struct GlfwInitializerFinalizer
{
	GlfwInitializerFinalizer() { ASSERTMSG(GLFW_TRUE == glfwInit(), "Failed to initialize GLFW library"); }
	~GlfwInitializerFinalizer() { glfwTerminate(); }
};

struct CanvasStyle
{
	uint32_t	width;
	uint32_t	height;
	float		minDepth;
	float		maxDepth;
	bool		visible;
	bool		resizable;
};


struct CanvasContext
{
	VkAllocationCallbacksPtr	cc_callbacks;
	VkDebugUtilsMessengerEXT	cc_debugMessenger;
	VkDebugReportCallbackEXT	cc_debugReport;
	ZInstance					cc_instance;
	ZGLFWwindowPtr				cc_window;
	ZSurfaceKHR					cc_surface;
	ZPhysicalDevice				cc_physicalDevice;
	ZDevice						cc_device;
	CanvasContext	(const char*		appName,
					 uint32_t			apiVersion,
					 uint32_t			engVersion,
					 uint32_t			appVersion,
					 const strings&		instanceLayers,
					 const strings&		instanceExtensions,
					 const strings&		deviceExtensions,
					 const CanvasStyle& style,
					 add_ptr<Canvas>	canvas);
};

class Canvas : public GlfwInitializerFinalizer, public CanvasContext, public VulkanContext
{
public:
	struct SurfaceDetails
	{
		VkSurfaceCapabilitiesKHR		caps;
		std::vector<VkSurfaceFormatKHR>	formats;
		std::vector<VkPresentModeKHR>	modes;
		SurfaceDetails ();
		void update (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
	};

	struct BackBuffer
	{
		uint32_t	imageIndex;
		ZSemaphore	acquireSemaphore;
		ZSemaphore	renderSemaphore;
		ZFence		presentFence;

		BackBuffer (VulkanContext& ctx);
		BackBuffer (const BackBuffer&);
		void destroy (VulkanContext&);
	};

	class SwapChain
	{
	public:
		struct Framebuffer
		{
			VkImage						image;
			VkImageView					view;
			std::optional<ZImageView>	depth;
			VkFramebuffer				framebuffer;
			Framebuffer ();
		};
		typedef std::vector<Framebuffer> FrameBuffers;
		SwapChain (Canvas&);
		~SwapChain ();
		const VkSwapchainKHR&	handle;
		const VkViewport&		viewport;
		const VkRect2D&			scissor;
		const VkExtent2D&		extent;
		const FrameBuffers&		buffers;
		const uint32_t&			bufferCount;
		const uint32_t&			refreshCount;
		void setup (ZRenderPass rp, uint32_t hintWidth = 0, uint32_t hintHeight = 0, bool force = false);
	private:
		void rebuild (ZRenderPass rp);
		void destroy (bool clear);
		Canvas&						m_canvas;
		VkSwapchainKHR				m_handle;
		FrameBuffers				m_buffers;
		uint32_t					m_refreshCount;
		VkViewport					m_viewport;
		VkRect2D					m_scissor;
		VkExtent2D					m_extent;
		uint32_t					m_bufferCount;
	};

	struct CommandAndFence
	{
		CommandAndFence (Canvas&);
		CommandAndFence (ZCommandPool);
		ZFence			fence;
		ZCommandBuffer	cmdBuffer;
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
	};

	static const CanvasStyle DefaultStyle; // 800,600,0,1,true,true

public:
	Canvas	(const char*		appName,
			 const strings&		instanceLayers		= {},
			 const strings&		instanceExtensions	= {},
			 const strings&		deviceExtensions	= {},
			 const CanvasStyle&	style				= DefaultStyle,
			 uint32_t			apiVersion			= VK_API_VERSION_1_0,
			 uint32_t			engVersion			= VK_MAKE_VERSION(1, 0, 0),
			 uint32_t			appVersion			= VK_MAKE_VERSION(1, 0, 0));
	virtual ~Canvas	();

	ZQueue					getPresentQueue () const;
	uint32_t				getPresentQueueFamilyIndex () const;
	ZGLFWwindowPtr&			window;
	ZSurfaceKHR&			surface;
	const SurfaceDetails&	surfaceDetails;
	VkSurfaceFormatKHR&		format;
	const SwapChain&		swapchain;
	const uint32_t&			width;
	const uint32_t&			height;
	const CanvasStyle&		style;
	GLFWEvents&				events();

	VkRenderPassBeginInfo	makeRenderPassBeginInfo	(ZRenderPass rp, uint32_t swapImageIndex) const;
	void					transitionImageForPresent (ZCommandBuffer cmdBuffer, uint32_t swapImageIndex);
	typedef std::function<bool (Canvas& canvas, ZCommandBuffer cmdBuffer, uint32_t swapImageIndex)> OnCommandRecordingCallback;
	int						run					(ZRenderPass					rp,
												 OnCommandRecordingCallback		onCommandRecording,
												 std::reference_wrapper<int>	= m_drawTrigger);

	typedef std::function<void(Canvas&, void*, uint64_t)> TimerCallback;
	void					setTimer			(TimerCallback callback, void* userData, uint64_t milliseconds);
	template<class F> bool	userToWindow		(const Area<F>& userArea, const VecX<F,2>& userPoint, VecX<F,2>& windowPoint) const;
	template<class F> bool	windowToUser		(const Area<F>& userArea, const VecX<F,2>& windowPoint, VecX<F,2>& userPoint) const;

protected:

	BackBuffer			acquireBackBuffer		(ZRenderPass rp);
	void				presentBackBuffer		(ZRenderPass rp, const BackBuffer& backBuffer);
	void				releaseBackBuffers		();

	friend struct GLFWEvents;

	SurfaceDetails					m_surfaceDetails;
	VkSurfaceFormatKHR				m_format;
	uint32_t						m_width;
	uint32_t						m_height;
	CanvasStyle						m_style;
	std::deque<BackBuffer>			m_backBuffers;
	std::vector<CommandAndFence>	m_commandFences;
	SwapChain						m_swapChain;
	std::unique_ptr<GLFWEvents>		m_events;
	TimerCallback					m_timerCallback;
	void*							m_timerUserData;
	uint64_t						m_timerPeriodMS;
	static int						m_drawTrigger;
};

template<class Float>
bool Canvas::userToWindow (const Canvas::Area<Float>& userArea, const VecX<Float,2>& userPoint, VecX<Float,2>& windowPoint) const
{
	windowPoint.x( ((userPoint.x() - userArea.xMin) / userArea.width()) * float(width) );
	windowPoint.y( ((userPoint.y() - userArea.yMin) / userArea.height()) * float(height) );
	return true;
}

template<class Float>
bool Canvas::windowToUser (const Canvas::Area<Float>& userArea, const VecX<Float,2>& windowPoint, VecX<Float,2>& userPoint) const
{
	userPoint.x( (windowPoint.x() / static_cast<Float>(width)) * userArea.width() + userArea.xMin );
	userPoint.y( (windowPoint.y() / static_cast<Float>(height)) * userArea.height() + userArea.yMin );
	return true;
}

template<class Float> Float Canvas::Area<Float>::width () const { return xMax - xMin; }
template<class Float> Float Canvas::Area<Float>::height () const { return yMax - yMin; }
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
