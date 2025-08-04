#ifndef __VTF_STRUCT_UTILS_HPP_INCLUDED__
#define __VTF_STRUCT_UTILS_HPP_INCLUDED__

#include "vulkan/vulkan.h"
#include "vtfZDeletable.hpp"
#include "vtfCUtils.hpp"
#include <typeinfo>
#include <variant>

namespace vtf
{

template<class> std::bad_typeid mkstype;

//#define MKSTYPE_SELECT(what_, vkstructtype_, vkstructname_)
//	MKSTYPE_SELECT_IMPL(what_, vkstructtype_, vkstructname_)

//#define MKSTYPE_SELECT_IMPL(what_, vkstructtype_, vkstructname_)
//	MKSTYPE_SELECT_##what_(vkstructtype_, vkstructname_)

#define MKSTYPE(vkstructtype_, vkstructname_) \
	template<> inline constexpr VkStructureType	mkstype<vkstructtype_> = vkstructname_

#define MKSTYPE_SELECT(what_, vkstructtype_, vkstructname_) \
	MKSTYPE_SELECT_##what_(vkstructtype_, vkstructname_)

#define MKSTYPE_SELECT_DEF(vkstructtype_, vkstructname_) \
	template<> constexpr inline VkStructureType mkstype<vkstructtype_> = vkstructname_;

#define MKSTYPE_EXTRACT(what_, vkstructtype_, vkstructname_) \
	MKSTYPE_EXTRACT_##what_(vkstructtype_, vkstructname_)

#define MKSTYPE_EXTRACT_DEF(vkstructtype_, vkstructname_) ,vkstructtype_

//#define MKSTYPE_SELECT_DEFINE_TYPE(vkstructtype_, vkstructname_)
//	template<> constexpr VkStructureType mkstype<vkstructtype_> = vkstructname_;

//#define MKSTYPE_SELECT_EXTRACT_TYPE(vkstructtype_, vkstructname_) ,vkstructtype_

//#define APPLY_DEFINE_TYPE(vkstructtype_, vkstructname_)
//	MKSTYPE_SELECT(DEFINE_TYPE, vkstructtype_, vkstructname_)

//#define APPLY_EXTRACT_TYPE(vkstructtype_, vkstructname_)
//    MKSTYPE_SELECT(EXTRACT_TYPE, vkstructtype_, vkstructname_)

#define FEATURES_LIST(APPLY) \
APPLY(DEF, VkValidationFeaturesEXT, VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) \
APPLY(DEF, VkPhysicalDeviceVulkan11Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) \
APPLY(DEF, VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) \
APPLY(DEF, VkPhysicalDeviceVulkan13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) \
APPLY(DEF, VkPhysicalDeviceMultiviewFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES) \
APPLY(DEF, VkPhysicalDeviceMaintenance4Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES) \
APPLY(DEF, VkPhysicalDeviceShaderObjectFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDeviceDynamicRenderingFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES) \
APPLY(DEF, VkPhysicalDeviceSynchronization2Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES) \
APPLY(DEF, VkPhysicalDeviceSubgroupSizeControlFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES) \
APPLY(DEF, VkPhysicalDeviceExtendedDynamicStateFeaturesEXT,	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDeviceShaderTerminateInvocationFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES) \
APPLY(DEF, VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES) \
APPLY(DEF, VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR,	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR) \
APPLY(DEF, VkPhysicalDeviceDescriptorBufferFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDeviceBufferDeviceAddressFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES) \
APPLY(DEF, VkPhysicalDeviceBufferAddressFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES) \
APPLY(DEF, VkPhysicalDeviceColorWriteEnableFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT) \
APPLY(DEF, VkPhysicalDevice16BitStorageFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES) \
APPLY(DEF, VkPhysicalDevice8BitStorageFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES) \
APPLY(DEF, VkPhysicalDeviceCooperativeMatrixFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR) \
APPLY(DEF, VkPhysicalDeviceCooperativeMatrixFeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV) \
APPLY(DEF, VkPhysicalDeviceCooperativeMatrix2FeaturesNV, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_2_FEATURES_NV) \
APPLY(DEF, VkPhysicalDeviceMeshShaderFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT) \

//APPLY(DEF, VkPhysicalDeviceFeatures, VK_STRUCTURE_TYPE_MAX_ENUM)
MKSTYPE(VkPhysicalDeviceFeatures, VK_STRUCTURE_TYPE_MAX_ENUM);

//#define EXTRACT_TYPES() FEATURES_LIST(APPLY_EXTRACT_TYPE)
//#define DEFINE_FEATURES() FEATURES_LIST(APPLY_DEFINE_TYPE)

// VkBaseInStructure -> VK_STRUCTURE_TYPE_BASE_IN_STRUCTURE
// VkBaseInStructure -> VK_STRUCTURE_TYPE_BASE_IN_STRUCTURE
FEATURES_LIST(MKSTYPE_SELECT)

#define EXTRACT_TYPES() FEATURES_LIST(MKSTYPE_EXTRACT)
//using FeaturesVar = RemoveTypeFromContainer_t< int, std::variant<int EXTRACT_TYPES()> >;
using FeaturesVar = std::variant<VkPhysicalDeviceFeatures EXTRACT_TYPES()>;
//using FeaturesVar = RemoveTypeFromContainer_t<int, FeaturesVar_>;

MKSTYPE(VkPhysicalDeviceProperties2,				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
MKSTYPE(VkPhysicalDeviceMultiviewProperties,		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES);
MKSTYPE(VkRenderPassBeginInfo,						VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
MKSTYPE(VkApplicationInfo,							VK_STRUCTURE_TYPE_APPLICATION_INFO);
MKSTYPE(VkMemoryAllocateInfo,						VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
MKSTYPE(VkMappedMemoryRange,						VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
MKSTYPE(VkRenderingAttachmentInfo,					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
MKSTYPE(VkRenderingInfo,							VK_STRUCTURE_TYPE_RENDERING_INFO);
MKSTYPE(VkPhysicalDeviceSubgroupProperties,			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
MKSTYPE(VkPhysicalDeviceMaintenance4Properties,		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES);
MKSTYPE(VkPipelineShaderStageRequiredSubgroupSizeCreateInfo,
													VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO);
MKSTYPE(VkPhysicalDeviceShaderObjectPropertiesEXT,  VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT);
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
MKSTYPE(VkShaderCreateInfoEXT,						VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT);
MKSTYPE(VkSamplerCreateInfo,						VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
MKSTYPE(VkFramebufferCreateInfo,					VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
MKSTYPE(VkRenderPassCreateInfo,						VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
MKSTYPE(VkRenderPassMultiviewCreateInfo,			VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
MKSTYPE(VkDeviceQueueCreateInfo,					VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
MKSTYPE(VkDeviceCreateInfo,							VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
MKSTYPE(VkFenceCreateInfo,							VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
MKSTYPE(VkSemaphoreCreateInfo,						VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
MKSTYPE(VkInstanceCreateInfo,						VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
MKSTYPE(VkDebugUtilsMessengerCreateInfoEXT,			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
MKSTYPE(VkDebugReportCallbackCreateInfoEXT,			VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT);
MKSTYPE(VkPipelineShaderStageCreateInfo,			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
MKSTYPE(VkComputePipelineCreateInfo,				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
MKSTYPE(VkDescriptorPoolCreateInfo,					VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
MKSTYPE(VkDescriptorSetLayoutCreateInfo,			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
MKSTYPE(VkDescriptorSetAllocateInfo,				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
MKSTYPE(VkWriteDescriptorSet,						VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
MKSTYPE(VkPipelineLayoutCreateInfo,					VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
MKSTYPE(VkCommandBufferInheritanceInfo,				VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
MKSTYPE(VkCommandBufferBeginInfo,					VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
MKSTYPE(VkSubmitInfo,								VK_STRUCTURE_TYPE_SUBMIT_INFO);
MKSTYPE(VkBindSparseInfo,							VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
MKSTYPE(VkDeviceGroupBindSparseInfo,				VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO);
MKSTYPE(VkDependencyInfo,							VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
MKSTYPE(VkBufferCopy2,								VK_STRUCTURE_TYPE_BUFFER_COPY_2);
MKSTYPE(VkCopyBufferInfo2,							VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2);
MKSTYPE(VkImageCopy2,								VK_STRUCTURE_TYPE_IMAGE_COPY_2);
MKSTYPE(VkCopyImageInfo2,							VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2);
MKSTYPE(VkBufferImageCopy2,							VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2);
MKSTYPE(VkCopyImageToBufferInfo2,					VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2);
MKSTYPE(VkCopyBufferToImageInfo2,					VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2);
MKSTYPE(VkVertexInputAttributeDescription2EXT,		VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT);
MKSTYPE(VkVertexInputBindingDescription2EXT,		VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT);
MKSTYPE(VkMemoryAllocateFlagsInfo,					VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
MKSTYPE(VkBufferDeviceAddressInfo,					VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
MKSTYPE(VkDescriptorAddressInfoEXT,					VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT);
MKSTYPE(VkDescriptorBufferBindingInfoEXT,			VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT);
MKSTYPE(VkDescriptorBufferBindingPushDescriptorBufferHandleEXT, VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT);
MKSTYPE(VkDescriptorGetInfoEXT,						VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT);
MKSTYPE(VkBufferCaptureDescriptorDataInfoEXT,		VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT);
MKSTYPE(VkImageCaptureDescriptorDataInfoEXT,		VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT);
MKSTYPE(VkImageViewCaptureDescriptorDataInfoEXT,	VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT);
MKSTYPE(VkSamplerCaptureDescriptorDataInfoEXT,		VK_STRUCTURE_TYPE_SAMPLER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT);
MKSTYPE(VkOpaqueCaptureDescriptorDataCreateInfoEXT,	VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT);
MKSTYPE(VkAccelerationStructureCaptureDescriptorDataInfoEXT, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT);
MKSTYPE(VkPhysicalDeviceDescriptorBufferPropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT);
MKSTYPE(VkCooperativeMatrixPropertiesKHR,			VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR);

struct makeVkStruct
{
	void* m_pNext;
	makeVkStruct (void* pNext = nullptr)
		: m_pNext(pNext) { }
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

template<class VkStructName>
struct VkStruct : public VkStructName
{
	inline constexpr VkStruct(add_ptr<void> pNext = nullptr)
		: VkStructName()
	{
		add_ref<VkStructName>(*this) = makeVkStruct(pNext);
	}
};

template<class, class = void>
struct hasPnextOfVoidPtr : std::false_type {};
template<class X> struct hasPnextOfVoidPtr<X, std::void_t<decltype(std::declval<X>().pNext)>>
	: std::integral_constant<bool,
		std::is_same<decltype(std::declval<X>().pNext), void*>::value ||
		std::is_same<decltype(std::declval<X>().pNext), const void*>::value> {};

#undef MKSTYPE_EXTRACT_DEF
#undef MKSTYPE_SELECT_DEF
#undef MKSTYPE_EXTRACT
#undef MKSTYPE_SELECT
#undef EXTRACT_TYPES
#undef FEATURES_LIST
#undef MKSTYPE

} // namespace vtf

#endif // __VTF_STRUCT_UTILS_HPP_INCLUDED__
