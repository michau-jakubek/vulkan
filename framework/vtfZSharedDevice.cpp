#include "vtfZSharedDevice.hpp"
#include "vtfStructUtils.hpp"
#include "vtfDebugMessenger.hpp"

namespace vtf
{
extern strings			getGlfwRequiredInstanceExtensions ();
extern ZGLFWwindowPtr	createWindow (const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer);
extern ZSurfaceKHR		createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window);

SharedDevice::SharedDevice ()
	: m_globalAppFlags	(getGlobalAppFlags())
	, m_callbacks		(getAllocationCallbacks())
	, m_debugMessenger	()
	, m_debugReport		()
	, m_glfw			(ZInstance(), false)
	, m_graphical		(false)
{
}

SharedDevice::SharedDevice (add_cptr<char> name, bool graphical)
	: m_globalAppFlags	(getGlobalAppFlags())
	, m_callbacks		(getAllocationCallbacks())
	, m_debugMessenger	()
	, m_debugReport		()
	, m_glfw			(ZInstance(), false)
	, m_graphical		(graphical)
{
	ZInstance instance = createInstance(name, m_callbacks,
									m_globalAppFlags.layers, {/*instanceExtensions*/},
									m_globalAppFlags.apiVer, m_globalAppFlags.debugPrintfEnabled);
	ZPhysicalDevice physDev = selectPhysicalDevice(make_signed(m_globalAppFlags.physicalDeviceIndex), instance, {/*deviceExtensions*/});
	VkPhysicalDeviceFeatures2 resultFeatures = makeVkStruct();
	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		UNREF(extensions);
		vkGetPhysicalDeviceFeatures2(*physicalDevice, &resultFeatures);
		return resultFeatures;
	};

	ZGLFWwindowPtr	window;
	ZSurfaceKHR		surface;
	if (graphical)
	{
		m_glfw.init(instance);
		window = createWindow(Canvas::DefaultStyle, name, this);
		surface = createSurface(instance, m_callbacks, window);
	}
	*this = createLogicalDevice(physDev, onEnablingFeatures, surface, m_globalAppFlags.debugPrintfEnabled);
}

SharedDevice::~SharedDevice()
{
	ZInstance i = getParam<ZPhysicalDevice>().getParam<ZInstance>();
	destroyDebugMessenger(i, m_callbacks, m_debugMessenger);
	destroyDebugReport(i, m_callbacks, m_debugReport);
}

} // namespace vtf
