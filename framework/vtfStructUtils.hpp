#ifndef __VTF_STRUCT_UTILS_HPP_INCLUDED__
#define __VTF_STRUCT_UTILS_HPP_INCLUDED__

#include "vulkan/vulkan.h"
#include <typeinfo>

namespace vtf
{

template<class> std::bad_typeid mkstype;

#define MKSTYPE(vkstructtype_, vkstructname_) template<> inline constexpr \
	VkStructureType	mkstype<vkstructtype_> = vkstructname_

MKSTYPE(VkPhysicalDeviceProperties2,				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
MKSTYPE(VkPhysicalDeviceFeatures2,					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
MKSTYPE(VkPhysicalDeviceMultiviewProperties,		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES);
MKSTYPE(VkPhysicalDeviceMultiviewFeatures,			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
MKSTYPE(VkValidationFeaturesEXT,					VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
MKSTYPE(VkPhysicalDeviceDynamicRenderingFeatures,	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
MKSTYPE(VkRenderPassBeginInfo,						VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
MKSTYPE(VkApplicationInfo,							VK_STRUCTURE_TYPE_APPLICATION_INFO);
MKSTYPE(VkMemoryAllocateInfo,						VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
MKSTYPE(VkMappedMemoryRange,						VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
MKSTYPE(VkRenderingAttachmentInfo,					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
MKSTYPE(VkRenderingInfo,							VK_STRUCTURE_TYPE_RENDERING_INFO);

MKSTYPE(VkPipelineInputAssemblyStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
MKSTYPE(VkPipelineVertexInputStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
MKSTYPE(VkPipelineTessellationStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
MKSTYPE(VkPipelineViewportStateCreateInfo,			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
MKSTYPE(VkPipelineRasterizationStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
MKSTYPE(VkPipelineMultisampleStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
MKSTYPE(VkPipelineDepthStencilStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
MKSTYPE(VkPipelineColorBlendStateCreateInfo,		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
MKSTYPE(VkPipelineDynamicStateCreateInfo,			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
MKSTYPE(VkGraphicsPipelineCreateInfo,				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
MKSTYPE(VkShaderModuleCreateInfo,					VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
MKSTYPE(VkFramebufferCreateInfo,					VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
MKSTYPE(VkRenderPassCreateInfo,						VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
MKSTYPE(VkRenderPassMultiviewCreateInfo,			VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
MKSTYPE(VkDeviceQueueCreateInfo,					VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
MKSTYPE(VkDeviceCreateInfo,							VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
MKSTYPE(VkFenceCreateInfo,							VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
MKSTYPE(VkSemaphoreCreateInfo,						VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
MKSTYPE(VkInstanceCreateInfo,						VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);


struct makeVkStruct
{
	void* m_pNext;
	makeVkStruct (void* pNext = nullptr)
		: m_pNext(pNext) {}
	template<class VKStructure>
	operator VKStructure () {
		VKStructure s{};
		s.sType = mkstype<VKStructure>;
		s.pNext = m_pNext;
		return s;
	}
};

template<class VKStructure>
auto makeVkStructT (void *pNext = nullptr) -> VKStructure
{
	struct makeVkStruct maker(pNext);
	return maker.operator VKStructure();
}

} // namespace vtf

#endif // __VTF_STRUCT_UTILS_HPP_INCLUDED__