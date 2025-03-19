#ifndef __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
#define __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfCanvas.hpp"
#include "vtfZShaderObject.hpp"

namespace vtf
{

class VertexInput;

/**
 * @brief The OneShotCommandBuffer RAII class
 * @note  After creation internal command buffer is in recording state
 *        so do not call vkBeginCommandBuffer once again.
 *        To finish recording call endRecordingAndSubmit() within the scope,
 *        otherwise the destructor will perform this task, so do not call
 *        vkEndCommandBuffer as well.
 */
class OneShotCommandBuffer
{
	bool			m_submitted;
	ZCommandBuffer	m_commandBuffer;

public:
	const bool&				submitted;
	const ZCommandBuffer&	commandBuffer;
	const ZCommandPool&		commandPool;

	OneShotCommandBuffer	(ZCommandPool pool);
	OneShotCommandBuffer	(ZDevice device, ZQueue queue);
	OneShotCommandBuffer	(const OneShotCommandBuffer&) = delete;
	OneShotCommandBuffer	(OneShotCommandBuffer&&) = delete;
	OneShotCommandBuffer&	operator=(const OneShotCommandBuffer&) = delete;
	~OneShotCommandBuffer	() { endRecordingAndSubmit(); }

	void endRecordingAndSubmit (ZFence hintFence = {}, uint64_t timeout = INVALID_UINT64);
};
std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool);

ZCommandPool	createCommandPool (ZDevice device, ZQueue queue, VkCommandPoolCreateFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
ZCommandBuffer	allocateCommandBuffer (ZCommandPool commandPool, bool primary = true, const void* pNext = nullptr);
ZCommandBuffer	createCommandBuffer (ZCommandPool commandPool, bool primary = true, const void* pNext = nullptr);
ZCommandPool	commandBufferGetCommandPool (ZCommandBuffer commandBuffer);
void			commandBufferEnd (ZCommandBuffer commandBuffer);
void			commandBufferBegin	(ZCommandBuffer commandBuffer,
									VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
									add_ptr<void> pNext = nullptr);
// If commandBuffer is secondary command buffer then VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT will be automatically added
void			commandBufferBegin (ZCommandBuffer commandBuffer, ZFramebuffer fb, ZRenderPass rp, uint32_t subpass = 0u,
									VkCommandBufferUsageFlags usage = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
									add_ptr<void> pNext = nullptr, add_ptr<void> pInhNext = nullptr);
void			commandBufferExecuteCommands (ZCommandBuffer primary, std::initializer_list<ZCommandBuffer> secondaryCommands);
VkResult		commandBufferSubmitAndWait (ZCommandBuffer commandBuffer, ZFence hintFence = ZFence(), uint64_t timeout = INVALID_UINT64,
											bool assertWaitResult = true);
/**
 * Binds a pipeline to a command buffer.
 * Implicitly binds any descritor sets if these
 * have been created when pipeline layout was created.
 */
void			commandBufferBindPipeline (ZCommandBuffer cmd, ZPipeline pipeline, bool bindDescriptorSets = true);
void			commandBufferBindDescriptorSets (ZCommandBuffer cmd, ZPipelineLayout layout,
												VkPipelineBindPoint bindingPoint);
void			commandBufferBindVertexBuffers (ZCommandBuffer cmd, add_cref<VertexInput> input,
												std::initializer_list<ZBuffer> externalBuffers = {},
												std::initializer_list<VkDeviceSize> offsets = {});
void			commandBufferSetVertexInputEXT (ZCommandBuffer cmd, add_cref<VertexInput> input);
void			commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkDeviceSize offset = 0);
void			commandBufferBindDescriptorBuffers (ZCommandBuffer, ZPipeline, std::initializer_list<ZBuffer>);
void			commandBufferDispatch (ZCommandBuffer cmd, const UVec3& workGroupCount = UVec3(1,1,1));

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZFramebuffer framebuffer,
												   uint32_t subpass, VkSubpassContents = VK_SUBPASS_CONTENTS_INLINE);
ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer,
												   uint32_t subpass, VkSubpassContents = VK_SUBPASS_CONTENTS_INLINE);
bool				 commandBufferNextSubpass (add_ref<ZRenderPassBeginInfo> beginInfo);
void				 commandBufferEndRenderPass (add_cref<ZRenderPassBeginInfo> beginInfo);
void				 commandBufferBeginRendering (ZCommandBuffer cmd, std::initializer_list<ZImageView> attachments,
												  std::optional<std::vector<VkClearValue>> clearColors = {},
												  ZRenderingFlags renderingFlags = ZRenderingFlags());
void				 commandBufferEndRendering (ZCommandBuffer cmd);
void				 commandBufferSetViewport (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain);
void				 commandBufferSetScissor (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain);
void				 commandBufferSetDefaultDynamicStates (ZCommandBuffer cmdBuffer, add_cref<VertexInput> vertexInput,
															add_cref<VkViewport> viewport, add_cptr<VkRect2D> pScissor = nullptr);

void				commandBufferDrawIndirect (ZCommandBuffer cmd, ZBuffer buffer);
void				commandBufferDrawIndexedIndirect (ZCommandBuffer cmd, ZBuffer buffer);

struct ZQueryPoolBeginInfo
{
	ZCommandBuffer	cmd;
	ZQueryPool		pool;
	uint32_t		query;
};
void				commandBufferResetQueryPool (ZCommandBuffer cmd, ZQueryPool queryPool);
ZQueryPoolBeginInfo	commandBufferBeginQuery (ZCommandBuffer cmd, ZQueryPool pool, uint32_t query, VkQueryControlFlags = 0);
void				commandBufferEndQuery (add_cref<ZQueryPoolBeginInfo> queryPoolBeginInfo);

template<std::size_t I>
bool __verifyPushConstants (add_cref<std::vector<type_index_with_default>>,
							add_cref<std::vector<VkPushConstantRange>>)
{
	return true;
}
template<std::size_t I, class PC__>
bool __verifyPushConstants (add_cref<std::vector<type_index_with_default>> types,
							add_cref<std::vector<VkPushConstantRange>> ranges,
							add_cref<PC__>)
{
	return (ranges.at(I).size == sizeof(PC__))
		&& (types.at(I) == type_index_with_default::make<PC__>());
}
template<std::size_t I, class PC__, class... OtherPCS>
bool __verifyPushConstants (add_cref<std::vector<type_index_with_default>> types,
							add_cref<std::vector<VkPushConstantRange>> ranges,
							const PC__& pc, const OtherPCS&... pcs)
{
	return (I < types.size() ? __verifyPushConstants<I, PC__>(types, ranges, pc) : true)
		&& (((I + 1u) < types.size()) ? __verifyPushConstants<I + 1u, OtherPCS...>(types, ranges, pcs...) : true);
}

template<class... PC__>
void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout,
								 std::initializer_list<ZShaderObject> shaders, PC__&&... pc)
{
	std::array<add_cptr<void>, sizeof...(PC__)> pValues{ &pc... };
	std::array<std::size_t, sizeof...(PC__)> sizes{ sizeof(PC__)... };
	std::array<type_index_with_default, sizeof...(PC__)>
		types{ type_index_with_default::make<std::remove_reference_t<PC__>>()... };

	extern bool verifyPushConstants (std::initializer_list<ZShaderObject> shaders,
									 std::size_t count,
									 add_cptr<std::size_t> sizes,
									 add_cptr<type_index_with_default> types);
	ASSERTMSG(verifyPushConstants(shaders, sizeof...(PC__), sizes.data(), types.data()),
									"Push constant types must match all the shaders types");

	extern void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout,
											std::initializer_list<ZShaderObject> shaders,
											std::size_t count, add_cptr<add_cptr<void>> pValues);
	commandBufferPushConstants(cmd, layout, shaders, sizeof...(PC__), pValues.data());
}

template<class... PC__>
void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout, PC__&&... pc)
{
	std::array<add_cptr<void>, sizeof...(PC__)> pValues{ &pc... };
	std::array<std::size_t, sizeof...(PC__)> sizes{ sizeof(PC__)... };
	std::array<type_index_with_default, sizeof...(PC__)>
		types{ type_index_with_default::make<std::remove_reference_t<PC__>>()... };

	extern bool verifyPushConstants (ZPipelineLayout layout,
									 std::size_t count,
									 add_cptr<std::size_t> sizes,
									 add_cptr<type_index_with_default> types);
	ASSERTMSG(verifyPushConstants(layout, sizeof...(PC__), sizes.data(), types.data()),
									"Push constant types do not match pieline layout");

	extern void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout,
											std::size_t count, add_cptr<add_cptr<void>> pValues);
	commandBufferPushConstants(cmd, layout, sizeof...(PC__), pValues.data());
}

void commandBufferBindShaders (ZCommandBuffer cmd, std::initializer_list<ZShaderObject> shaders);
void commandBufferUnbindShaders (ZCommandBuffer cmd, std::initializer_list<ZShaderObject> shaders);
void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image, add_cref<VkClearColorValue> clearValue,
									VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);
void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image, add_cref<VkClearColorValue> clearValue,
									add_cref<VkImageSubresourceRange> range, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);
void commandBufferBlitImage			(ZCommandBuffer cmd, ZImage srcImage, ZImage dstImage, VkFilter = VK_FILTER_LINEAR);
void commandBufferBlitImage			(ZCommandBuffer cmd, ZImage srcImage, ZImage dstImage,
									 VkImageSubresourceLayers srcSubresource,
									 VkImageSubresourceLayers dstSubresource,
									 VkFilter = VK_FILTER_LINEAR);
void commandBufferMakeImagePresentationReady (ZCommandBuffer cmdBuffer, ZImage image,
											  VkAccessFlags srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
											  VkAccessFlags dstAccess = VK_ACCESS_MEMORY_READ_BIT,
											  VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
											  VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

template<class Barrier, class... Barriers>
void commandBufferPipelineBarriers (ZCommandBuffer cmd,
									VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
									Barrier&& barrier, Barriers&&... barriers)
{
	VkMemoryBarrier			memoryBarriers	[sizeof...(barriers) + 1];
	VkImageMemoryBarrier	imageBarriers	[sizeof...(barriers) + 1];
	VkBufferMemoryBarrier	bufferBarriers	[sizeof...(barriers) + 1];
	BarriersInfo	info
	{
		memoryBarriers,
		imageBarriers,
		bufferBarriers,
		0u, 0u, 0u
	};
	pushBarriers(info, std::forward<Barrier>(barrier), std::forward<Barriers>(barriers)...);
	vkCmdPipelineBarrier(*cmd, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
						 info.memoryBarrierCount, (info.memoryBarrierCount ? memoryBarriers : nullptr),
						 info.bufferBarrierCount, (info.bufferBarrierCount ? bufferBarriers : nullptr),
						 info.imageBarrierCount, (info.imageBarrierCount ? imageBarriers : nullptr));
}

template<class Barrier, class... Barriers>
void commandBufferPipelineBarriers2 (ZCommandBuffer		cmd,
									 VkDependencyFlags	dependencyFlags,
									 Barrier&& barrier, Barriers&&... barriers)
{
	VkMemoryBarrier2		memoryBarriers	[sizeof...(barriers) + 1];
	VkImageMemoryBarrier2	imageBarriers	[sizeof...(barriers) + 1];
	VkBufferMemoryBarrier2	bufferBarriers	[sizeof...(barriers) + 1];

	BarriersInfo2 info
	{
		memoryBarriers,
		imageBarriers,
		bufferBarriers,
		0u, 0u, 0u
	};

	pushBarriers(info, std::forward<Barrier>(barrier), std::forward<Barriers>(barriers)...);

	extern void doCommandBufferPipelineBarriers2 (ZCommandBuffer			cmd,
												  add_cref<BarriersInfo2>	info,
												  VkDependencyFlags			dependencyFlags);

	doCommandBufferPipelineBarriers2(cmd, info, dependencyFlags);
}

template<class Barrier, class... Barriers>
void commandBufferPipelineBarriers2 (ZCommandBuffer	cmd,
									 Barrier&& barrier, Barriers&&... barriers)
{
	commandBufferPipelineBarriers2<Barrier, Barriers...>(cmd, VK_DEPENDENCY_BY_REGION_BIT,
								   std::forward<Barrier>(barrier), std::forward<Barriers>(barriers)...);
}

} // namespace vtf

#endif // __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
