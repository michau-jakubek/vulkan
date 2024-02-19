#ifndef __VTF_SHARED_DEVICE_HPP_INCLUDED__
#define __VTF_SHARED_DEVICE_HPP_INCLUDED__

#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfCanvas.hpp"

namespace vtf
{
struct SharedDevice : public expander<SharedDevice, ZDevice>, public ZDevice, public GlfwInitializerFinalizer
{
	using expander<SharedDevice, ZDevice>::operator=;
	add_cref<GlobalAppFlags>	m_globalAppFlags;
	VkAllocationCallbacksPtr	m_callbacks;
	VkDebugUtilsMessengerEXT	m_debugMessenger;
	VkDebugReportCallbackEXT	m_debugReport;
	const bool					m_graphical;
	SharedDevice();
	SharedDevice(add_cptr<char> name, bool graphical);
	virtual ~SharedDevice();
};
} // namespace 

#endif // __VTF_SHARED_DEVICE_HPP_INCLUDED__
