#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfCUtils.hpp"

#include <iostream>

namespace vtf
{

ZInstanceInterface ZInstanceSingleton::intf;
const ZInstanceInterface& ZInstanceSingleton::getInterface (VkInstance instance)
{
	intf.initialize(instance);
	return intf;
}

ZDeviceInterface ZDeviceSingleton::intf;
const ZDeviceInterface& ZDeviceSingleton::getInterface (VkInstance instance, VkDevice device)
{
	intf.initialize(instance, device);
	return intf;
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
