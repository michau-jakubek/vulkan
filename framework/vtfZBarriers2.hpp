#ifndef __VTF_ZBARRIERS_2_HPP_INCLUDED__
#define __VTF_ZBARRIERS_2_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{

struct ZBarrierConstants
{
	enum class Access : VkAccessFlagBits2
	{
		NONE                                          = VK_ACCESS_2_NONE,
		NONE_KHR                                      = VK_ACCESS_2_NONE_KHR,
		INDIRECT_COMMAND_READ_BIT                     = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
		INDIRECT_COMMAND_READ_BIT_KHR                 = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,
		INDEX_READ_BIT                                = VK_ACCESS_2_INDEX_READ_BIT,
		INDEX_READ_BIT_KHR                            = VK_ACCESS_2_INDEX_READ_BIT_KHR,
		VERTEX_ATTRIBUTE_READ_BIT                     = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
		VERTEX_ATTRIBUTE_READ_BIT_KHR                 = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR,
		UNIFORM_READ_BIT                              = VK_ACCESS_2_UNIFORM_READ_BIT,
		UNIFORM_READ_BIT_KHR                          = VK_ACCESS_2_UNIFORM_READ_BIT_KHR,
		INPUT_ATTACHMENT_READ_BIT                     = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
		INPUT_ATTACHMENT_READ_BIT_KHR                 = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT_KHR,
		SHADER_READ_BIT                               = VK_ACCESS_2_SHADER_READ_BIT,
		SHADER_READ_BIT_KHR                           = VK_ACCESS_2_SHADER_READ_BIT_KHR,
		SHADER_WRITE_BIT                              = VK_ACCESS_2_SHADER_WRITE_BIT,
		SHADER_WRITE_BIT_KHR                          = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
		COLOR_ATTACHMENT_READ_BIT                     = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
		COLOR_ATTACHMENT_READ_BIT_KHR                 = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR,
		COLOR_ATTACHMENT_WRITE_BIT                    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		COLOR_ATTACHMENT_WRITE_BIT_KHR                = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
		DEPTH_STENCIL_ATTACHMENT_READ_BIT             = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR         = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR,
		DEPTH_STENCIL_ATTACHMENT_WRITE_BIT            = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR        = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR,
		TRANSFER_READ_BIT                             = VK_ACCESS_2_TRANSFER_READ_BIT,
		TRANSFER_READ_BIT_KHR                         = VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
		TRANSFER_WRITE_BIT                            = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		TRANSFER_WRITE_BIT_KHR                        = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
		HOST_READ_BIT                                 = VK_ACCESS_2_HOST_READ_BIT,
		HOST_READ_BIT_KHR                             = VK_ACCESS_2_HOST_READ_BIT_KHR,
		HOST_WRITE_BIT                                = VK_ACCESS_2_HOST_WRITE_BIT,
		HOST_WRITE_BIT_KHR                            = VK_ACCESS_2_HOST_WRITE_BIT_KHR,
		MEMORY_READ_BIT                               = VK_ACCESS_2_MEMORY_READ_BIT,
		MEMORY_READ_BIT_KHR                           = VK_ACCESS_2_MEMORY_READ_BIT_KHR,
		MEMORY_WRITE_BIT                              = VK_ACCESS_2_MEMORY_WRITE_BIT,
		MEMORY_WRITE_BIT_KHR                          = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
		SHADER_SAMPLED_READ_BIT                       = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		SHADER_SAMPLED_READ_BIT_KHR                   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR,
		SHADER_STORAGE_READ_BIT                       = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
		SHADER_STORAGE_READ_BIT_KHR                   = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR,
		SHADER_STORAGE_WRITE_BIT                      = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		SHADER_STORAGE_WRITE_BIT_KHR                  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR,
		TRANSFORM_FEEDBACK_WRITE_BIT_EXT              = VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
		TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT       = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
		TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT      = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
		CONDITIONAL_RENDERING_READ_BIT_EXT            = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT,
		COMMAND_PREPROCESS_READ_BIT_NV                = VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV,
		COMMAND_PREPROCESS_WRITE_BIT_NV               = VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV,
		FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR = VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
		SHADING_RATE_IMAGE_READ_BIT_NV                = VK_ACCESS_2_SHADING_RATE_IMAGE_READ_BIT_NV,
		ACCELERATION_STRUCTURE_READ_BIT_KHR           = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		ACCELERATION_STRUCTURE_WRITE_BIT_KHR          = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		ACCELERATION_STRUCTURE_READ_BIT_NV            = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_NV,
		ACCELERATION_STRUCTURE_WRITE_BIT_NV           = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
		FRAGMENT_DENSITY_MAP_READ_BIT_EXT             = VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
		COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT     = VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
		INVOCATION_MASK_READ_BIT_HUAWEI               = VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI,
		SHADER_BINDING_TABLE_READ_BIT_KHR             = VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR,
	};
	static_assert(sizeof(VkPipelineStageFlags2) == sizeof(Access), "???");

	enum class Stage : VkPipelineStageFlagBits2
	{
		NONE =                                                          VK_PIPELINE_STAGE_2_NONE,
		NONE_KHR =                                                      VK_PIPELINE_STAGE_2_NONE_KHR,
		TOP_OF_PIPE_BIT =                                               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		TOP_OF_PIPE_BIT_KHR =                                           VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
		DRAW_INDIRECT_BIT =                                             VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		DRAW_INDIRECT_BIT_KHR =                                         VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR,
		VERTEX_INPUT_BIT =                                              VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
		VERTEX_INPUT_BIT_KHR =                                          VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR,
		VERTEX_SHADER_BIT =                                             VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		VERTEX_SHADER_BIT_KHR =                                         VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR,
		TESSELLATION_CONTROL_SHADER_BIT =                               VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
		TESSELLATION_CONTROL_SHADER_BIT_KHR =                           VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT_KHR,
		TESSELLATION_EVALUATION_SHADER_BIT =                            VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
		TESSELLATION_EVALUATION_SHADER_BIT_KHR =                        VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT_KHR,
		GEOMETRY_SHADER_BIT =                                           VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
		GEOMETRY_SHADER_BIT_KHR =                                       VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT_KHR,
		FRAGMENT_SHADER_BIT =                                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		FRAGMENT_SHADER_BIT_KHR =                                       VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
		EARLY_FRAGMENT_TESTS_BIT =                                      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		EARLY_FRAGMENT_TESTS_BIT_KHR =                                  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR,
		LATE_FRAGMENT_TESTS_BIT =                                       VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		LATE_FRAGMENT_TESTS_BIT_KHR =                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR,
		COLOR_ATTACHMENT_OUTPUT_BIT =                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		COLOR_ATTACHMENT_OUTPUT_BIT_KHR =                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
		COMPUTE_SHADER_BIT =                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		COMPUTE_SHADER_BIT_KHR =                                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
		ALL_TRANSFER_BIT =                                              VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
		ALL_TRANSFER_BIT_KHR =                                          VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR,
		TRANSFER_BIT =                                                  VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		TRANSFER_BIT_KHR =                                              VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
		BOTTOM_OF_PIPE_BIT =                                            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		BOTTOM_OF_PIPE_BIT_KHR =                                        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
		HOST_BIT =                                                      VK_PIPELINE_STAGE_2_HOST_BIT,
		HOST_BIT_KHR =                                                  VK_PIPELINE_STAGE_2_HOST_BIT_KHR,
		ALL_GRAPHICS_BIT =                                              VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
		ALL_GRAPHICS_BIT_KHR =                                          VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
		ALL_COMMANDS_BIT =                                              VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		ALL_COMMANDS_BIT_KHR =                                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
		COPY_BIT =                                                      VK_PIPELINE_STAGE_2_COPY_BIT,
		COPY_BIT_KHR =                                                  VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
		RESOLVE_BIT =                                                   VK_PIPELINE_STAGE_2_RESOLVE_BIT,
		RESOLVE_BIT_KHR =                                               VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR,
		BLIT_BIT =                                                      VK_PIPELINE_STAGE_2_BLIT_BIT,
		BLIT_BIT_KHR =                                                  VK_PIPELINE_STAGE_2_BLIT_BIT_KHR,
		CLEAR_BIT =                                                     VK_PIPELINE_STAGE_2_CLEAR_BIT,
		CLEAR_BIT_KHR =                                                 VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR,
		INDEX_INPUT_BIT =                                               VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
		INDEX_INPUT_BIT_KHR =                                           VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR,
		VERTEX_ATTRIBUTE_INPUT_BIT =                                    VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
		VERTEX_ATTRIBUTE_INPUT_BIT_KHR =                                VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR,
		PRE_RASTERIZATION_SHADERS_BIT =                                 VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
		PRE_RASTERIZATION_SHADERS_BIT_KHR =                             VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR,
		TRANSFORM_FEEDBACK_BIT_EXT =                                    VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,
		CONDITIONAL_RENDERING_BIT_EXT =                                 VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,
		COMMAND_PREPROCESS_BIT_NV =                                     VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV,
		FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR =                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
		SHADING_RATE_IMAGE_BIT_NV =                                     VK_PIPELINE_STAGE_2_SHADING_RATE_IMAGE_BIT_NV,
		ACCELERATION_STRUCTURE_BUILD_BIT_KHR =                          VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		RAY_TRACING_SHADER_BIT_KHR =                                    VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
		RAY_TRACING_SHADER_BIT_NV =                                     VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_NV,
		ACCELERATION_STRUCTURE_BUILD_BIT_NV =		                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
		FRAGMENT_DENSITY_PROCESS_BIT_EXT =                              VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
		TASK_SHADER_BIT_NV =                                            VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV,
		MESH_SHADER_BIT_NV =                                            VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV,
		SUBPASS_SHADING_BIT_HUAWEI =                                    VK_PIPELINE_STAGE_2_SUBPASS_SHADING_BIT_HUAWEI,
		INVOCATION_MASK_BIT_HUAWEI =                                    VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI,
		ACCELERATION_STRUCTURE_COPY_BIT_KHR =                           VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR,
	};
	static_assert(sizeof(VkPipelineStageFlags2) == sizeof(Stage), "???");
};

struct ZMemoryBarrier2 : protected VkMemoryBarrier2, public ZBarrierConstants
{
	ZMemoryBarrier2 (add_ptr<void> pNext = nullptr);
	ZMemoryBarrier2 (Access srcAccess, Stage srcStage,
					 Access dstAccess, Stage dstStage, add_ptr<void> pNext = nullptr);
	VkMemoryBarrier2 operator ()();
};

struct ZBufferMemoryBarrier2 : protected VkBufferMemoryBarrier2, public ZBarrierConstants
{
	ZBufferMemoryBarrier2 (add_ptr<void> pNext = nullptr);
	ZBufferMemoryBarrier2 (ZBuffer buffer,	Access srcAccess, Stage srcStage,
											Access dstAccess, Stage dstStage, add_ptr<void> pNext = nullptr);
	VkBufferMemoryBarrier2 operator()();
	add_cref<ZBuffer>	   buffer () const { return m_buffer; }
protected:
	ZBuffer	m_buffer;
};

struct ZImageMemoryBarrier2 : protected VkImageMemoryBarrier2, public ZBarrierConstants
{
	ZImageMemoryBarrier2 (add_ptr<void> pNext = nullptr);
	ZImageMemoryBarrier2 (ZImage image, Access srcAccess, Stage srcStage,
										Access dstAccess, Stage dstStage,
										VkImageLayout targetLayout, add_ptr<void> pNext = nullptr);
	ZImageMemoryBarrier2 (ZImage image,	Access srcAccess, Stage srcStage,
										Access dstAccess, Stage dstStage,
										VkImageLayout targetLayout, add_cref<VkImageSubresourceRange>,
										add_ptr<void> pNext = nullptr);
	VkImageMemoryBarrier2 operator()();
	add_cref<ZImage>	  image () const { return m_image; }
protected:
	ZImage	m_image;
};

struct BarriersInfo2
{
	add_ptr<VkMemoryBarrier2>		pMemoryBarriers;
	add_ptr<VkImageMemoryBarrier2>	pImageBarriers;
	add_ptr<VkBufferMemoryBarrier2>	pBufferBarriers;
	uint32_t						memoryBarrierCount;
	uint32_t						imageBarrierCount;
	uint32_t						bufferBarrierCount;
};

void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZMemoryBarrier2> barrier);
void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZBufferMemoryBarrier2> barrier);
void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZImageMemoryBarrier2> barrier);

void pushBarriers (add_ref<BarriersInfo2>);
template<class Barrier, class... Barriers>
void pushBarriers (add_ref<BarriersInfo2> info, Barrier&& barrier, Barriers&&... barriers)
{
	pushKnownBarrier(info, barrier);
	pushBarriers(info, std::forward<Barriers>(barriers)...);
}

} // vtf

#endif // __VTF_ZBARRIERS_2_HPP_INCLUDED__
