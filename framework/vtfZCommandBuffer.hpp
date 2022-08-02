#ifndef __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
#define __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{

class VertexInput;

struct OneShotCommandBuffer
{
	const bool&				submitted;
	const ZCommandBuffer&	commandBuffer;

	OneShotCommandBuffer	(ZCommandPool pool);
	~OneShotCommandBuffer	() { submit(); }

	void submit ();

private:
	bool			m_submitted;
	ZCommandPool	m_commandPool;
	ZCommandBuffer	m_commandBuffer;
};
std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool);

ZCommandPool	createCommandPool(ZDevice device, ZQueue queue, VkCommandPoolCreateFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
ZCommandBuffer	createCommandBuffer (ZCommandPool commandPool);
void			commandBufferEnd (ZCommandBuffer commandBuffer);
void			commandBufferBegin (ZCommandBuffer commandBuffer);
void			commandBufferSubmitAndWait (ZCommandBuffer commandBuffer);
void			commandBufferSubmitAndWait (std::initializer_list<ZCommandBuffer> commandBuffers);
void			commandBufferBindPipeline(ZCommandBuffer cmd, ZPipeline pipeline);
void			commandBufferBindVertexBuffers (ZCommandBuffer cmd, const VertexInput& input, std::initializer_list<ZBuffer> externalBuffers = {});
void			commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkIndexType indexType);

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

} // namespace vtf

#endif // __VTF_ZCOMMANDBUFFER_HPP_INCLUDED__
