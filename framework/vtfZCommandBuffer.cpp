#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZImage.hpp"
#include <memory>

namespace vtf
{

ZCommandPool createCommandPool (ZDevice device, ZQueue queue, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex	= queue.getParam<uint32_t>();
	poolInfo.flags				= flags;

	VkCommandPool				pool = VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks = device.getParam<VkAllocationCallbacksPtr>();

	if (vkCreateCommandPool(*device, &poolInfo, callbacks, &pool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}

	return ZCommandPool::create(pool, device, callbacks, queue);
}

ZCommandBuffer createCommandBuffer (ZCommandPool commandPool)
{
	VkCommandBuffer	commandBuffer	= VK_NULL_HANDLE;
	auto			device			= commandPool.getParam<ZDevice>();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool			= *commandPool;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount	= 1;

	VKASSERT2(vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer));

	return ZCommandBuffer::create(commandBuffer, device, commandPool);
}

void commandBufferEnd (ZCommandBuffer commandBuffer)
{
	VKASSERT2(vkEndCommandBuffer(*commandBuffer));
}

void commandBufferBegin (ZCommandBuffer commandBuffer)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext				= nullptr;
	beginInfo.flags				= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	beginInfo.pInheritanceInfo	= nullptr;

	VKASSERT2(vkBeginCommandBuffer(*commandBuffer, &beginInfo));
}

void commandBufferSubmitAndWait (ZCommandBuffer commandBuffer)
{
	commandBufferSubmitAndWait( { commandBuffer } );
}

void commandBufferSubmitAndWait (std::initializer_list<ZCommandBuffer> commandBuffers)
{
	const uint32_t commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	ASSERTION(commandBufferCount);

	auto	ii		= commandBuffers.begin();
	ZDevice	device	= ii->getParam<ZDevice>();

	std::vector<VkSubmitInfo>	submits(commandBufferCount);
	std::vector<VkFence>		fences(commandBufferCount);
	std::vector<ZFence>			zfences;

	for (auto i = ii; i != commandBuffers.end(); ++i)
	{
		ASSERTION(	device== i->getParam<ZDevice>()	);
		auto at = make_unsigned(std::distance(ii, i));

		submits[at].sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submits[at].pNext				= nullptr;

		submits[at].commandBufferCount	= 1;
		submits[at].pCommandBuffers		= i->ptr();

		submits[at].waitSemaphoreCount	= 0;
		submits[at].pWaitSemaphores		= nullptr;
		submits[at].pWaitDstStageMask	= nullptr;

		submits[at].signalSemaphoreCount	= 0;
		submits[at].pSignalSemaphores	= nullptr;

		zfences.emplace_back(createFence(device));
		fences[at] = *zfences.back();
	}

	for (auto i = ii; i != commandBuffers.end(); ++i)
	{
		auto at = make_unsigned(std::distance(ii, i));
		ZQueue queue = i->getParam<ZCommandPool>().getParam<ZQueue>();
		if (vkQueueSubmit(*queue, 1, &submits[at], fences[at]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit command buffer!");
		}
	}

	if (vkWaitForFences(*device, commandBufferCount, fences.data(), VK_TRUE, INVALID_UINT64) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to wait for command buffer!");
	}
}

void commandBufferBindPipeline (ZCommandBuffer cmd, ZPipeline pipeline)
{
	auto bindingPoint		= pipeline.getParam<VkPipelineBindPoint>();
	auto pipelineLayout		= pipeline.getParam<ZPipelineLayout>();
	auto& descriptorLayouts	= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();

	std::vector<VkDescriptorSet> sets;
	std::transform(descriptorLayouts.begin(), descriptorLayouts.end(), std::back_inserter(sets),
				   [](const ZDescriptorSetLayout& dsl) { return **dsl.getParam<std::optional<ZDescriptorSet>>(); });

	vkCmdBindPipeline(*cmd, bindingPoint, *pipeline);

	if (sets.size())
	{
		vkCmdBindDescriptorSets(*cmd,
								bindingPoint,
								*pipelineLayout,
								0,				//firstSet
								static_cast<uint32_t>(sets.size()),
								sets.data(),
								0,				//dynamicOffsetCount
								nullptr);		//pDynamicOffsets
	}
}

void commandBufferBindVertexBuffers (ZCommandBuffer cmd, const VertexInput& input,
									std::initializer_list<ZBuffer> externalBuffers,
									std::initializer_list<VkDeviceSize> offsets)
{
	const std::vector<VkBuffer>				vertexBuffers	= input.getVertexBuffers(externalBuffers);
	const std::vector<VkBuffer>::size_type	count			= vertexBuffers.size();
	std::vector<VkDeviceSize>				vertexOffsets	(count, 0u);
	for (auto k = offsets.begin(), i = k; i != offsets.end(); ++i)
	{
		auto j = make_unsigned(std::distance(k, i));
		if (j < count)
			vertexOffsets[j] = *i;
		else break;
	}
	vkCmdBindVertexBuffers(*cmd, 0, static_cast<uint32_t>(count), vertexBuffers.data(), vertexOffsets.data());
}

void commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkIndexType indexType, VkDeviceSize offset)
{
	vkCmdBindIndexBuffer(*cmd, *buffer, offset, indexType);
}

void commandBufferPushConstants(ZCommandBuffer cmd , ZPipelineLayout layout, VkShaderStageFlags flags, uint32_t offset, uint32_t size, const void* p)
{
	vkCmdPushConstants(*cmd, *layout, flags, offset, size, p);
}

void commandBufferDispatch (ZCommandBuffer cmd, const UVec3& workGroupCount)
{
	ZDevice								device	= cmd.getParam<ZDevice>();
	ZPhysicalDevice						phys	= device.getParam<ZPhysicalDevice>();
	add_ref<VkPhysicalDeviceProperties>	props	= phys.getParamRef<VkPhysicalDeviceProperties>();
	ASSERTION( (workGroupCount.x() <= props.limits.maxComputeWorkGroupCount[0])
			&& (workGroupCount.y() <= props.limits.maxComputeWorkGroupCount[1])
			&& (workGroupCount.z() <= props.limits.maxComputeWorkGroupCount[2]) );
	vkCmdDispatch(*cmd, workGroupCount.x(), workGroupCount.y(), workGroupCount.z());
}

void commandBufferSetPolygonModeEXT(ZCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
	UNREF(commandBuffer);
	UNREF(polygonMode);
//	static PFN_vkCmdSetPolygonModeEXT cmdSetPolygonModeEXT;
//	if (nullptr == cmdSetPolygonModeEXT)
//	{
//		ZInstance instance = commandBuffer.getParam<ZDevice>().getParam<ZPhysicalDevice>().getParam<ZInstance>();
//		cmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT) vkGetInstanceProcAddr(*instance, "vkCmdSetPolygonModeEXT");
//	}
//	(*cmdSetPolygonModeEXT)(*commandBuffer, polygonMode);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, VkImage image, const VkClearColorValue& clearValue,
								   VkImageLayout srclayout, VkImageLayout dstlayout,
								   VkAccessFlags	srcAccessMask, VkAccessFlags	dstAccessMask,
								   VkPipelineStageFlags	srcStage, VkPipelineStageFlags	dstStage,
								   const VkImageSubresourceRange& range)
{
	//VkImageSubresourceRange		range{};
	//range.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT;
	//range.baseMipLevel			= 0u;
	//range.levelCount			= 1u;
	//range.baseArrayLayer		= 0u;
	//range.layerCount			= 1u;

	VkImageMemoryBarrier		before{};
	before.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	before.pNext				= nullptr;
	before.oldLayout			= srclayout;
	before.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	before.srcAccessMask		= srcAccessMask;
	before.dstAccessMask		= VK_ACCESS_TRANSFER_WRITE_BIT;
	before.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	before.image				= image;
	before.subresourceRange		= range;

	VkImageMemoryBarrier		after{};
	after.sType					= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	after.pNext					= nullptr;
	after.oldLayout				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	after.newLayout				= dstlayout;
	after.srcAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
	after.dstAccessMask			= dstAccessMask;
	after.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	after.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	after.image					= image;
	after.subresourceRange		= range;

	vkCmdPipelineBarrier(*cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, //VK_DEPENDENCY_BY_REGION_BIT,
						 0, (const VkMemoryBarrier*)nullptr,
						 0, (const VkBufferMemoryBarrier*)nullptr,
						 1, &before);
	vkCmdClearColorImage(*cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &range);

	vkCmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, 0, //VK_DEPENDENCY_BY_REGION_BIT,
						 0, (const VkMemoryBarrier*)nullptr,
						 0, (const VkBufferMemoryBarrier*)nullptr,
						 1, &after);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image,
								   const VkClearColorValue&	clearValue,
								   VkImageLayout			dstlayout,
								   VkAccessFlags			srcAccessMask,
								   VkAccessFlags			dstAccessMask,
								   VkPipelineStageFlags		srcStage,
								   VkPipelineStageFlags		dstStage)
{
	add_cref<VkImageCreateInfo>		info	= image.getParamRef<VkImageCreateInfo>();
	const VkImageSubresourceRange	range	= makeImageSubresourceRange(image);
	commandBufferClearColorImage(cmd, *image, clearValue, info.initialLayout, dstlayout, srcAccessMask, dstAccessMask, srcStage, dstStage, range);
	changeImageLayout(image, dstlayout);
}

template<> void commandBufferPipelineBarrier<VkBufferMemoryBarrier>	(ZCommandBuffer cmd,
																	 const VkBufferMemoryBarrier& barrier,
																	 VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	vkCmdPipelineBarrier(*cmd, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
						 0u, nullptr,	// memory
						 1u, &barrier,
						 0u, nullptr);	// image
}

template<> void commandBufferPipelineBarrier<VkImageMemoryBarrier>	(ZCommandBuffer cmd,
																	 const VkImageMemoryBarrier& barrier,
																	 VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	vkCmdPipelineBarrier(*cmd, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT,
						 0u, nullptr,	// memory
						 0u, nullptr,	// buffer
						 1u, &barrier);	// image
}

OneShotCommandBuffer::OneShotCommandBuffer (ZCommandPool pool)
	: m_submitted		(false)
	, m_commandBuffer	(createCommandBuffer(pool))
	, submitted			(m_submitted)
	, commandBuffer		(m_commandBuffer)
	, commandPool		(m_commandBuffer.getParamRef<ZCommandPool>())
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(*commandBuffer, &beginInfo);
}

OneShotCommandBuffer::OneShotCommandBuffer (ZDevice device, ZQueue queue)
	: OneShotCommandBuffer(createCommandPool(device, queue))
{
}

void OneShotCommandBuffer::submit ()
{
	if (!m_submitted)
	{
		auto queue = commandPool.getParam<ZQueue>();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = m_commandBuffer.ptr();

		vkEndCommandBuffer(*m_commandBuffer);
		vkQueueSubmit(*queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(*queue);

		m_submitted = true;
	}
}

std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool)
{
	std::unique_ptr<OneShotCommandBuffer> u(new OneShotCommandBuffer(commandPool));
	return u;
}

} // namespace vtf
