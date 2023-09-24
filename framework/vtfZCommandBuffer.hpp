#ifndef __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
#define __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfCanvas.hpp"

namespace vtf
{

class VertexInput;

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
	~OneShotCommandBuffer	() { submit(); }

	void submit ();
};
std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool);

ZCommandPool	createCommandPool(ZDevice device, ZQueue queue, VkCommandPoolCreateFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
ZCommandBuffer	allocateCommandBuffer (ZCommandPool commandPool, bool primary = true, const void* pNext = nullptr);
ZCommandBuffer	createCommandBuffer (ZCommandPool commandPool, bool primary = true, const void* pNext = nullptr);
ZCommandPool	commandBufferGetCommandPool (ZCommandBuffer commandBuffer);
void			commandBufferEnd (ZCommandBuffer commandBuffer);
void			commandBufferBegin (ZCommandBuffer commandBuffer);
void			commandBufferSubmitAndWait (ZCommandBuffer commandBuffer, ZFence hintFence = ZFence(), uint64_t timeout = INVALID_UINT64);
/**
 * Binds a pipeline to a command buffer.
 * Implicitly binds any descritor sets if these
 * have been created when pipeline layout was created.
 */
void			commandBufferBindPipeline(ZCommandBuffer cmd, ZPipeline pipeline);
void			commandBufferBindVertexBuffers (ZCommandBuffer cmd, const VertexInput& input,
												std::initializer_list<ZBuffer> externalBuffers = {},
												std::initializer_list<VkDeviceSize> offsets = {});
void			commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkDeviceSize offset = 0);
void			commandBufferDispatch (ZCommandBuffer cmd, const UVec3& workGroupCount = UVec3(1,1,1));
void			commandBufferSetPolygonModeEXT (ZCommandBuffer commandBuffer, VkPolygonMode polygonMode);

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZFramebuffer framebuffer, uint32_t subpass);
ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer, uint32_t subpass);
bool				 commandBufferNextSubpass (add_ref<ZRenderPassBeginInfo> beginInfo);
void				 commandBufferEndRenderPass (add_cref<ZRenderPassBeginInfo> beginInfo);
void				 commandBufferBeginRendering (ZCommandBuffer cmd, std::initializer_list<ZImageView> attachments,
												  std::optional<std::vector<VkClearValue>> clearColors,
												  ZRenderingFlags renderingFlags = ZRenderingFlags());
void				 commandBufferEndRendering (ZCommandBuffer cmd);
void				 commandBufferSetViewport (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain);
void				 commandBufferSetScissor (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain);

template<class PC__>
void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout, const PC__& pc)
{
	std::type_index typePushConstant = layout.getParam<type_index_with_default>();
	const VkPushConstantRange& range = layout.getParamRef<VkPushConstantRange>();
	ASSERTION(std::type_index(typeid(PC__)) == typePushConstant);
	ASSERTION(range.size == sizeof(PC__));
	extern void commandBufferPushConstants(ZCommandBuffer, ZPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
	commandBufferPushConstants(cmd, layout, range.stageFlags, range.offset, range.size, &pc);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image, add_cref<VkClearColorValue> clearValue);
void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image,
								   add_cref<VkClearColorValue> clearValue, add_cref<VkImageSubresourceRange> range);
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

} // namespace vtf

#endif // __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
