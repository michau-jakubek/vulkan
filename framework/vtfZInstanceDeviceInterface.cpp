#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfCUtils.hpp"

#include <iostream>

namespace vtf
{

ZInstanceInterface ZInstanceSingleton::m_interface;
ZInstanceSingleton::~ZInstanceSingleton ()
{
	add_ptr<ZInstance> me = static_cast<add_ptr<ZInstance>>(this);
	if (me->use_count() == 1 && me->has_handle())
	{
		destroyDebugMessenger(*me);
		destroyDebugReport(*me);
	}
}

const ZInstanceInterface& ZInstanceSingleton::initInterface (VkInstance instance)
{
	m_interface.initialize(instance);
	return m_interface;
}

const ZInstanceInterface& ZInstanceSingleton::getInterface() const
{
	ASSERTMSG(m_interface.initialized, "Device interface is not initialized");
	return m_interface;
}

ZDeviceInterface ZDeviceSingleton::m_interface;
const ZDeviceInterface& ZDeviceSingleton::initInterface (VkInstance instance, VkDevice device)
{
	m_interface.initialize(instance, device);
	return m_interface;
}

const ZDeviceInterface& ZDeviceSingleton::getInterface () const
{
	ASSERTMSG(m_interface.initialized, "Device interface is not initialized");
	return m_interface;
}

void ZInstanceInterface::initialize (VkInstance instance)
{
	if (false == initialized)
	{
		initialized = true;
		const PFN_vkGetInstanceProcAddr instProc = getDriverGetInstanceProcAddr();
		ASSERTMSG(instProc, "vkGetInstanceProcAddr() failed");
		init(instance, instProc);
	}
}

PFN_vkDestroyShaderEXT ZDeviceInterface::mDestroyShaderEXT;

void ZDeviceInterface::initialize (VkInstance instance, VkDevice device)
{
	if (initialized) return;
	const PFN_vkGetDeviceProcAddr devProc = getDriverGetDeviceProcAddr();
	const PFN_vkGetInstanceProcAddr instProc = getDriverGetInstanceProcAddr();
	init(instance, instProc, device, devProc);
	mDestroyShaderEXT = this->vkDestroyShaderEXT;
	initialized = true;
}

bool ZDeviceInterface::isShaderObjectEnabled () const
{
	return true
		&& this->vkCreateShadersEXT != nullptr
		&& this->vkDestroyShaderEXT != nullptr
		&& this->vkGetShaderBinaryDataEXT != nullptr
		&& this->vkCmdBindShadersEXT != nullptr;
}

} // namespace vtf
