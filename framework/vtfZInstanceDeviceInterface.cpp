#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfDebugMessenger.hpp"
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

const ZInstanceInterface& ZInstanceSingleton::getInterface (VkInstance instance)
{
	m_interface.initialize(instance);
	return m_interface;
}

ZDeviceInterface ZDeviceSingleton::m_interface;
const ZDeviceInterface& ZDeviceSingleton::getInterface (VkInstance instance, VkDevice device)
{
	m_interface.initialize(instance, device);
	return m_interface;
}

void ZInstanceInterface::initialize (VkInstance instance)
{
	UNREF(instance);
	if (initialized) return;
	initialized = true;
}

PFN_vkDestroyShaderEXT ZDeviceInterface::mDestroyShaderEXT;

void ZDeviceInterface::initialize (VkInstance instance, VkDevice device)
{
	if (initialized) return;
	init(instance, &::vkGetInstanceProcAddr, device, &::vkGetDeviceProcAddr);
	mDestroyShaderEXT = this->vkDestroyShaderEXT;
	initialized = true;
	//__PFN_vkCreateShadersEXT = (PFN_vkCreateShadersEXT)
	//	vkGetDeviceProcAddr(device, "vkCreateShadersEXT");
	//__PFN_vkDestroyShaderEXT = (PFN_vkDestroyShaderEXT)
	//	vkGetDeviceProcAddr(device, "vkDestroyShaderEXT");
	//__PFN_vkGetShaderBinaryDataEXT = (PFN_vkGetShaderBinaryDataEXT)
	//	vkGetDeviceProcAddr(device, "vkGetShaderBinaryDataEXT");
	//__PFN_vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)
	//	vkGetDeviceProcAddr(device, "vkCmdBindShadersEXT");
	//__PFN_vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)
	//	vkGetDeviceProcAddr(device, "vkCmdSetVertexInputEXT");

	//// VK_EXT_extended_dynamic_state
	//__PFN_vkCmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)
	//	vkGetDeviceProcAddr(device, "vkCmdSetScissorWithCountEXT");
	//__PFN_vkCmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)
	//	vkGetDeviceProcAddr(device, "vkCmdSetViewportWithCountEXT");
}

bool ZDeviceInterface::shaderObject () const
{
	return true
		&& this->vkCreateShadersEXT != nullptr
		&& this->vkDestroyShaderEXT != nullptr
		&& this->vkGetShaderBinaryDataEXT != nullptr
		&& this->vkCmdBindShadersEXT != nullptr;
}

} // namespace vtf
