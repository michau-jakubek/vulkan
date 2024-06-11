#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfStructUtils.hpp"
#include <memory>

namespace vtf
{

ZCommandPool createCommandPool (ZDevice device, ZQueue queue, VkCommandPoolCreateFlags flags)
{
	ASSERTMSG(queue.has_handle(), "Queue must have handle");
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex	= queueGetFamilyIndex(queue);
	poolInfo.flags				= flags;

	VkAllocationCallbacksPtr	callbacks = device.getParam<VkAllocationCallbacksPtr>();
	ZCommandPool				commandPool(VK_NULL_HANDLE, device, callbacks, queue);

	VKASSERT2(vkCreateCommandPool(*device, &poolInfo, callbacks, commandPool.setter()));

	return commandPool;
}

ZCommandBuffer allocateCommandBuffer (ZCommandPool commandPool, bool primary, const void* pNext)
{
	return createCommandBuffer(commandPool, primary, pNext);
}

ZCommandBuffer createCommandBuffer (ZCommandPool commandPool, bool primary, const void* pNext)
{
	VkCommandBuffer	commandBuffer	= VK_NULL_HANDLE;
	auto			device			= commandPool.getParam<ZDevice>();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext					= pNext;
	allocInfo.commandPool			= *commandPool;
	allocInfo.level					= primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount	= 1;

	VKASSERT2(vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer));

	return ZCommandBuffer::create(commandBuffer, device, commandPool, primary);
}

ZCommandPool commandBufferGetCommandPool (ZCommandBuffer commandBuffer)
{
	return commandBuffer.getParam<ZCommandPool>();
}

void commandBufferEnd (ZCommandBuffer commandBuffer)
{
	VKASSERT2(vkEndCommandBuffer(*commandBuffer));
}

void commandBufferBegin (ZCommandBuffer commandBuffer)
{
	VkCommandBufferInheritanceInfo	inheritInfo = makeVkStruct();
	inheritInfo.renderPass				= VK_NULL_HANDLE;
	inheritInfo.subpass					= 0;
	inheritInfo.framebuffer				= VK_NULL_HANDLE;
	inheritInfo.occlusionQueryEnable	= VK_FALSE;
	inheritInfo.queryFlags				= 0;
	inheritInfo.pipelineStatistics		= 0;

	VkCommandBufferBeginInfo	beginInfo = makeVkStruct();
	beginInfo.flags				= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	beginInfo.pInheritanceInfo	= commandBuffer.getParam<bool>() ? nullptr : &inheritInfo;

	VKASSERT2(vkBeginCommandBuffer(*commandBuffer, &beginInfo));
}

void commandBufferExecuteCommands (ZCommandBuffer primary, std::initializer_list<ZCommandBuffer> secondaryCommands)
{
	ASSERTMSG((secondaryCommands.size() > 0u && secondaryCommands.size() <= 256u),
			"\"secondaryCommands\" must not be empty and not bigger than 256");
	ASSERTMSG(primary.getParam<bool>(), "The first \"primary\" param must be primary command buffer");
	VkCommandBuffer commands[256];
	for (auto begin = secondaryCommands.begin(), i = begin; i != secondaryCommands.end(); ++i)
	{
		commands[std::distance(begin, i)] = **i;
		ASSERTMSG((false == i->getParam<bool>()),
			"\"secondaryCommands\" elements must be secondary command buffers");
	}

	vkCmdExecuteCommands(*primary, static_cast<uint32_t>(secondaryCommands.size()), commands);
}

void commandBufferSubmitAndWait (ZCommandBuffer commandBuffer, ZFence hintFence, uint64_t timeout)
{
	ZDevice			device		= commandBuffer.getParam<ZDevice>();
	ZQueue			queue		= commandBuffer.getParam<ZCommandPool>().getParam<ZQueue>();
	ZFence			fence		= hintFence.has_handle() ? hintFence : createFence(device);

	VkSubmitInfo	submitInfo	= makeVkStruct();
	submitInfo.commandBufferCount	= 1u;
	submitInfo.pCommandBuffers		= commandBuffer.ptr();

	VKASSERT2(vkQueueSubmit(*queue, 1u, &submitInfo, *fence));
	VKASSERT2(vkWaitForFences(*device, 1u, fence.ptr(), VK_TRUE, timeout));
	resetFence(fence);
}

void commandBufferBindPipeline (ZCommandBuffer cmd, ZPipeline pipeline)
{
	auto bindingPoint		= pipeline.getParam<VkPipelineBindPoint>();
	auto pipelineLayout		= pipeline.getParam<ZPipelineLayout>();
	add_ref<std::vector<ZDescriptorSetLayout>> descriptorLayouts
			= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();

	std::vector<VkDescriptorSet> sets(descriptorLayouts.size());
	std::transform(descriptorLayouts.begin(), descriptorLayouts.end(), sets.begin(),
				   [](const ZDescriptorSetLayout& dsl) { return *dsl.getParam<ZDescriptorSet>(); });

	vkCmdBindPipeline(*cmd, bindingPoint, *pipeline);

	if (sets.size())
	{
		vkCmdBindDescriptorSets(*cmd,
								bindingPoint,
								*pipelineLayout,
								0,				//firstSet
								data_count(sets),
								data_or_null(sets),
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

void commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkDeviceSize offset)
{
	ASSERTMSG((buffer.getParamRef<VkBufferCreateInfo>().usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
			  "Buffer must be created with VK_BUFFER_USAGE_INDEX_BUFFER_BIT");
	const type_index_with_default type = buffer.getParam<type_index_with_default>();
	VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
	if (type == type_index_with_default::make<uint32_t>())
		indexType = VK_INDEX_TYPE_UINT32;
	else if (type == type_index_with_default::make<uint16_t>())
		indexType = VK_INDEX_TYPE_UINT16;
	else { ASSERTMSG(false, "Unknown index type"); }
	vkCmdBindIndexBuffer(*cmd, *buffer, offset, indexType);
}

void commandBufferPushConstants (ZCommandBuffer cmd , ZPipelineLayout layout, VkShaderStageFlags flags,
								 uint32_t offset, uint32_t size, const void* p)
{
	ZDevice device = cmd.getParam<ZDevice>();
	ASSERTMSG((offset + size) <= deviceGetPhysicalLimits(device).maxPushConstantsSize,
			  "Push constant size exceeds device limit");
	vkCmdPushConstants(*cmd, *layout, flags, offset, size, p);
}

void commandBufferDispatch (ZCommandBuffer cmd, const UVec3& workGroupCount)
{
	ZDevice								device	= cmd.getParam<ZDevice>();
	ZPhysicalDevice						phys	= device.getParam<ZPhysicalDevice>();
	add_ref<VkPhysicalDeviceProperties>	props	= phys.getParamRef<VkPhysicalDeviceProperties>();
	ASSERTMSG( (workGroupCount.x() <= props.limits.maxComputeWorkGroupCount[0])
			&& (workGroupCount.y() <= props.limits.maxComputeWorkGroupCount[1])
			&& (workGroupCount.z() <= props.limits.maxComputeWorkGroupCount[2]),
			"Too much worgroup count passed to dispatch function");
	vkCmdDispatch(*cmd, workGroupCount.x(), workGroupCount.y(), workGroupCount.z());
}

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer, uint32_t subpass)
{
	const uint32_t	countOfAttachments	= renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>();
	const uint32_t	subpassCount		= renderPass.getParam<ZDistType<SubpassCount, uint32_t>>();
	const bool differs = framebuffer.getParamRef<std::vector<ZImageView>>().size() != countOfAttachments;
	ASSERTMSG(!differs, "Attachment count from renderPas and framebuffer differs");
	ASSERTMSG(subpass < subpassCount, "Subpass index exceeds renderPass subpasses amount");
	ZRenderPassBeginInfo	renderPassBegin(cmd, renderPass, framebuffer, subpass);
	VkRenderPassBeginInfo	info = renderPassBegin();
	vkCmdBeginRenderPass(*cmd, &info, VK_SUBPASS_CONTENTS_INLINE);	
	for (uint32_t i = 0; i < subpass; ++i)
	{
		vkCmdNextSubpass(*cmd, VK_SUBPASS_CONTENTS_INLINE);
	}
	return renderPassBegin;
}

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZFramebuffer framebuffer, uint32_t subpass)
{
	return commandBufferBeginRenderPass(cmd, framebuffer.getParam<ZRenderPass>(), framebuffer, subpass);
}

bool commandBufferNextSubpass (add_ref<ZRenderPassBeginInfo> beginInfo)
{
	const uint32_t subpassCount = beginInfo.getRenderPass().getParam<ZDistType<SubpassCount, uint32_t>>();
	if (beginInfo.getSubpass() < subpassCount)
	{
		vkCmdNextSubpass(*beginInfo.getCommandBuffer(), VK_SUBPASS_CONTENTS_INLINE);
		beginInfo.consumeSubpass();
		return true;
	}
	return false;
}

void commandBufferEndRenderPass (add_cref<ZRenderPassBeginInfo> beginInfo)
{
	ZCommandBuffer cmdBuffer = beginInfo.getCommandBuffer();
	const uint32_t subpassCount = beginInfo.getRenderPass().getParam<ZDistType<SubpassCount, uint32_t>>();
	for (uint32_t i = (beginInfo.getSubpass() + 1u); i < subpassCount; ++i)
	{
		vkCmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}
	vkCmdEndRenderPass(*cmdBuffer);
	const VkImageLayout finalLayout = beginInfo.getRenderPass().getParam<VkImageLayout>();
	ZFramebuffer fb = beginInfo.getFramebuffer();
	add_cref<std::vector<ZImageView>> views = fb.getParamRef<std::vector<ZImageView>>();
	for (add_cref<ZImageView> view : views)
	{
		imageResetLayout(view.getParam<ZImage>(), finalLayout);
	}
}

void commandBufferBeginRendering (ZCommandBuffer cmd, std::initializer_list<ZImageView> attachments,
								  std::optional<std::vector<VkClearValue>> clearColors, ZRenderingFlags renderingFlags)
{
	ASSERTMSG(attachments.size(), "There must be at least one attachment to perform rendering");
	const uint32_t			clearColorCount = clearColors.has_value() ? data_count(*clearColors) : 0u;
	add_cptr<VkClearValue>	clearColorsPtr	= clearColorCount ? data_or_null(*clearColors) : nullptr;

	std::vector<VkRenderingAttachmentInfo> attInfos(attachments.size());
	for (auto begin = attachments.begin(), i = begin; i != attachments.end(); ++i)
	{
		decltype(attInfos)::size_type k = make_unsigned(std::distance(begin, i));
		add_ref<VkRenderingAttachmentInfo> a = attInfos.at(k);
		a = makeVkStruct();

		a.imageView					= **i;
		a.imageLayout				= imageGetLayout(imageViewGetImage(*i));
		a.resolveMode				= VK_RESOLVE_MODE_NONE;
		a.resolveImageView			= VK_NULL_HANDLE;
		a.resolveImageLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		a.loadOp					= clearColorCount
										? (k < clearColorCount)
										  ?	VK_ATTACHMENT_LOAD_OP_CLEAR
										  : VK_ATTACHMENT_LOAD_OP_LOAD
										: VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.storeOp					= VK_ATTACHMENT_STORE_OP_STORE;
		if (k < clearColorCount)	a.clearValue = clearColorsPtr[k];
	}

	VkRenderingInfo rInfo = makeVkStruct();
	rInfo.flags					= renderingFlags();
	rInfo.renderArea			= makeRect2D(makeExtent2D(imageGetExtent(imageViewGetImage(*attachments.begin()))));
	rInfo.layerCount			= 1u;
	rInfo.viewMask				= 0b0;
	rInfo.colorAttachmentCount	= data_count(attInfos);
	rInfo.pColorAttachments		= data_or_null(attInfos);
	rInfo.pDepthAttachment		= nullptr;
	rInfo.pStencilAttachment	= nullptr;

	vkCmdBeginRendering(*cmd, &rInfo);
}

void commandBufferEndRendering (ZCommandBuffer cmd)
{
	vkCmdEndRendering(*cmd);
}

void commandBufferSetViewport (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain)
{
	vkCmdSetViewport(*cmd, 0u, 1u, &swapchain.viewport);
}

void commandBufferSetScissor (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain)
{
	vkCmdSetScissor(*cmd, 0u, 1u, &swapchain.scissor);
}

void commandBufferSetPolygonModeEXT (ZCommandBuffer commandBuffer, VkPolygonMode polygonMode)
{
	// TODO: body
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

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image, add_cref<VkClearColorValue> clearValue)
{
	commandBufferClearColorImage(cmd, image, clearValue, imageMakeSubresourceRange(image));
}

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image,
								   add_cref<VkClearColorValue> clearValue,
								   add_cref<VkImageSubresourceRange> range)
{
	const VkImageLayout layout = imageGetLayout(image);
	const VkImageLayout finalLayout = (layout == VK_IMAGE_LAYOUT_GENERAL
									   || layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
									   || layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
										? layout : VK_IMAGE_LAYOUT_GENERAL;
	ZImageMemoryBarrier prepare(image, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);
	ZImageMemoryBarrier verify(image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE,
								finalLayout, range);
	commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, prepare);
	vkCmdClearColorImage(*cmd, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &range);
	commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, verify);
}

void commandBufferBlitImage (ZCommandBuffer cmd, ZImage srcImage, ZImage dstImage, VkFilter filter)
{
	commandBufferBlitImage(cmd, srcImage, dstImage,
						   imageMakeSubresourceLayers(srcImage), imageMakeSubresourceLayers(dstImage),
						   filter);
}

void commandBufferBlitImage (ZCommandBuffer cmd, ZImage srcImage, ZImage dstImage,
							 VkImageSubresourceLayers srcSubresource, VkImageSubresourceLayers dstSubresource, VkFilter filter)
{
	VkImageBlit				blitRegion{};
	add_cref<VkExtent3D>	srcImageExtent = srcImage.getParamRef<VkImageCreateInfo>().extent;
	add_cref<VkExtent3D>	dstImageExtent = dstImage.getParamRef<VkImageCreateInfo>().extent;

	blitRegion.srcSubresource	= srcSubresource;
	blitRegion.dstSubresource	= dstSubresource;
	// blitRegion.srcOffsets[0] = blitRegion.dstOffsets[0] = 0 default zero initialized
	blitRegion.srcOffsets[1].z = blitRegion.dstOffsets[1].z = 1;
	blitRegion.srcOffsets[1].x = make_signed(srcImageExtent.width);
	blitRegion.srcOffsets[1].y = make_signed(srcImageExtent.height);
	blitRegion.dstOffsets[1].x = make_signed(dstImageExtent.width);
	blitRegion.dstOffsets[1].y = make_signed(dstImageExtent.height);

	vkCmdBlitImage(*cmd,
				   *srcImage, imageGetLayout(srcImage),
				   *dstImage, imageGetLayout(dstImage),
				   1, &blitRegion,
				   filter);
}

void commandBufferMakeImagePresentationReady (ZCommandBuffer cmdBuffer, ZImage image,
											  VkAccessFlags srcAccess, VkAccessFlags dstAccess,
											  VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	ZImageMemoryBarrier			preparePresent	(image, srcAccess, dstAccess, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	commandBufferPipelineBarriers(cmdBuffer, srcStageMask, dstStageMask, preparePresent);
}

OneShotCommandBuffer::OneShotCommandBuffer (ZCommandPool pool)
	: m_submitted		(false)
	, m_commandBuffer	(createCommandBuffer(pool))
	, submitted			(m_submitted)
	, commandBuffer		(m_commandBuffer)
	, commandPool		(m_commandBuffer.getParamRef<ZCommandPool>())
{
	commandBufferBegin(m_commandBuffer);
}

OneShotCommandBuffer::OneShotCommandBuffer (ZDevice device, ZQueue queue)
	: OneShotCommandBuffer(createCommandPool(device, queue))
{
}

void OneShotCommandBuffer::endRecordingAndSubmit (ZFence hintFence, uint64_t timeout)
{
	if (!m_submitted)
	{
		commandBufferEnd(m_commandBuffer);
		commandBufferSubmitAndWait(m_commandBuffer, hintFence, timeout);
		m_submitted = true;
	}
}

std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool)
{
	std::unique_ptr<OneShotCommandBuffer> u(new OneShotCommandBuffer(commandPool));
	return u;
}

} // namespace vtf
