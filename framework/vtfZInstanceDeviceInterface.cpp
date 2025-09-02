#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfCUtils.hpp"

#include <iostream>

namespace vtf
{

static ZCommonInterface commonInterface;

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
	add_ref<ZInstanceInterface> i = dynamic_cast<add_ref<ZInstanceInterface>>(commonInterface);
	i.initialize(instance);
	return i;
}

const ZInstanceInterface& ZInstanceSingleton::getInterface() const
{
	add_ref<ZInstanceInterface> i = dynamic_cast<add_ref<ZInstanceInterface>>(commonInterface);
	ASSERTMSG(i.initialized, "Device interface is not initialized");
	return i;
}

const ZDeviceInterface& ZDeviceSingleton::initInterface (VkInstance instance, VkDevice device)
{
	add_ref<ZDeviceInterface> i = dynamic_cast<add_ref<ZDeviceInterface>>(commonInterface);
	i.initialize(instance, device);
	return i;
}

const ZDeviceInterface& ZDeviceSingleton::getInterface () const
{
	add_ref<ZDeviceInterface> i = dynamic_cast<add_ref<ZDeviceInterface>>(commonInterface);
	ASSERTMSG(i.initialized, "Device interface is not initialized");
	return i;
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
