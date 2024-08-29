#ifndef __ZSHADER_OBJECT_DEF_HPP_INCLUDED__
#define __ZSHADER_OBJECT_DEF_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZPushConstants.hpp"
#include "vtfZSpecializationInfo.hpp"

namespace vtf
{

typedef ZDeletable<VkShaderEXT,
	decltype(ZDeviceInterface::vkDestroyShaderEXT), &ZDeviceInterface::zDestroyShaderEXT,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkShaderStageFlagBits,
	ZSpecializationInfo,
	std::vector<ZDescriptorSetLayout>,
	std::vector<VkPushConstantRange>>
ZShaderObject;

} // namespace vtf

#endif // __ZSHADER_OBJECT_DEF_HPP_INCLUDED__
