#ifndef __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
#define __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

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
ZCommandBuffer	createCommandBuffer (ZCommandPool commandPool);
void			commandBufferEnd (ZCommandBuffer commandBuffer);
void			commandBufferBegin (ZCommandBuffer commandBuffer);
void			commandBufferSubmitAndWait (ZCommandBuffer commandBuffer);
void			commandBufferSubmitAndWait (std::initializer_list<ZCommandBuffer> commandBuffers);
void			commandBufferBindPipeline(ZCommandBuffer cmd, ZPipeline pipeline);
void			commandBufferBindVertexBuffers (ZCommandBuffer cmd, const VertexInput& input,
												std::initializer_list<ZBuffer> externalBuffers = {},
												std::initializer_list<VkDeviceSize> offsets = {});
void			commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer,
												VkIndexType indexType = VK_INDEX_TYPE_UINT32, VkDeviceSize offset = 0);
void			commandBufferDispatch (ZCommandBuffer cmd, const UVec3& workGroupCount = UVec3(1,1,1));
void			commandBufferSetPolygonModeEXT(ZCommandBuffer commandBuffer, VkPolygonMode polygonMode);

template<class PC__>
void commandBufferPushConstants (ZCommandBuffer cmd, ZPipelineLayout layout, const PC__& pc)
{
	std::type_index typePushConstant = layout.getParam<std::type_index>();
	const VkPushConstantRange& range = layout.getParamRef<VkPushConstantRange>();
	ASSERTION(std::type_index(typeid(PC__)) == typePushConstant);
	ASSERTION(range.size == sizeof(PC__));
	extern void commandBufferPushConstants(ZCommandBuffer, ZPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
	commandBufferPushConstants(cmd, layout, range.stageFlags, range.offset, range.size, &pc);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, VkImage image,
								   const VkClearColorValue&			clearValue,
								   VkImageLayout					srclayout		= VK_IMAGE_LAYOUT_UNDEFINED,
								   VkImageLayout					dstlayout		= VK_IMAGE_LAYOUT_GENERAL,
								   VkAccessFlags					srcAccessMask	= 0,
								   VkAccessFlags					dstAccessMask	= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
								   VkPipelineStageFlags				srcStage		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								   VkPipelineStageFlags				dstStage		= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								   const VkImageSubresourceRange&	subResRange		= makeImageSubresourceRange());

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image,
								   const VkClearColorValue&	clearValue,
								   VkImageLayout			dstlayout		= VK_IMAGE_LAYOUT_GENERAL,
								   VkAccessFlags			srcAccessMask	= 0,
								   VkAccessFlags			dstAccessMask	= (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
								   VkPipelineStageFlags		srcStage		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								   VkPipelineStageFlags		dstStage		= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

template<class Barrier>
void commandBufferPipelineBarrier	(ZCommandBuffer cmd, const Barrier& barrier, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

} // namespace vtf

#endif // __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
