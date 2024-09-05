#ifndef __ZSHADER_OBJECT_DEF_HPP_INCLUDED__
#define __ZSHADER_OBJECT_DEF_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZPushConstants.hpp"
#include "vtfZSpecializationInfo.hpp"

// Some of the implementations advertise that VK_EXT_shader_object extension
// is available however after physical device fetures are achieved an examined
// field VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject still is VK_FALSE.
// It turns out that including below layer during a process of creating a device
// workarounds this issue.
#define VK_LAYER_KHRONOS_SHADER_OBJECT_NAME "VK_LAYER_KHRONOS_shader_object"

namespace vtf
{

typedef ZDeletable<VkShaderEXT,
	decltype(ZDeviceInterface::vkDestroyShaderEXT), &ZDeviceInterface::zDestroyShaderEXT,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkShaderStageFlagBits,
	ZSpecializationInfo,
	std::vector<ZDescriptorSetLayout>,
	std::vector<VkPushConstantRange>,
	std::vector<type_index_with_default>>
ZShaderObject;

} // namespace vtf

#endif // __ZSHADER_OBJECT_DEF_HPP_INCLUDED__
