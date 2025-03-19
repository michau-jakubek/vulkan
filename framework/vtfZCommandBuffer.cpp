#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfStructUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfZPipeline.hpp"
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

	VKASSERT(vkCreateCommandPool(*device, &poolInfo, callbacks, commandPool.setter()));

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

	VKASSERT(vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer));

	return ZCommandBuffer::create(commandBuffer, device, commandPool, primary);
}

ZCommandPool commandBufferGetCommandPool (ZCommandBuffer commandBuffer)
{
	return commandBuffer.getParam<ZCommandPool>();
}

void commandBufferEnd (ZCommandBuffer commandBuffer)
{
	VKASSERT(vkEndCommandBuffer(*commandBuffer));
}

void commandBufferBegin (ZCommandBuffer commandBuffer, VkCommandBufferUsageFlags usage, add_ptr<void> pNext)
{
	commandBufferBegin(commandBuffer, {}, {}, 0u, usage, pNext);
}

void commandBufferBegin (ZCommandBuffer commandBuffer, ZFramebuffer fb, ZRenderPass rp, uint32_t subpass,
						 VkCommandBufferUsageFlags usage, add_ptr<void> pNext, add_ptr<void> pInhNext)
{
	VkCommandBufferInheritanceInfo	inheritInfo = makeVkStruct();
	inheritInfo.renderPass				= rp.has_handle()
											? *rp
											: fb.has_handle()
												? *framebufferGetRenderPass(fb)
												: VK_NULL_HANDLE;
	inheritInfo.subpass					= subpass;
	inheritInfo.framebuffer				= fb.has_handle() ? *fb : VK_NULL_HANDLE;
	inheritInfo.occlusionQueryEnable	= VK_FALSE;
	inheritInfo.pNext					= pInhNext;
	inheritInfo.queryFlags				= 0;
	inheritInfo.pipelineStatistics		= 0;

	VkCommandBufferBeginInfo	beginInfo = makeVkStruct();
	beginInfo.flags	= usage; // by default VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	beginInfo.pNext = pNext;
	if (false == commandBuffer.getParam<bool>())
	{
		beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	}
	beginInfo.pInheritanceInfo	= commandBuffer.getParam<bool>() ? nullptr : &inheritInfo;

	VKASSERT(vkBeginCommandBuffer(*commandBuffer, &beginInfo));
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

VkResult commandBufferSubmitAndWait (ZCommandBuffer commandBuffer, ZFence hintFence, uint64_t timeout, bool assertWaitResult)
{
	ZDevice			device		= commandBuffer.getParam<ZDevice>();
	ZQueue			queue		= commandBuffer.getParam<ZCommandPool>().getParam<ZQueue>();
	ZFence			fence		= hintFence.has_handle() ? hintFence : createFence(device);

	VkSubmitInfo	submitInfo	= makeVkStruct();
	submitInfo.commandBufferCount	= 1u;
	submitInfo.pCommandBuffers		= commandBuffer.ptr();

	VKASSERT(vkQueueSubmit(*queue, 1u, &submitInfo, *fence));
	const VkResult waitResult = vkWaitForFences(*device, 1u, fence.ptr(), VK_TRUE, timeout);
	if (assertWaitResult) VKASSERT(waitResult);
	if (hintFence.has_handle() == false)
	{
		resetFence(fence);
	}

	return waitResult;
}

void commandBufferBindDescriptorSets (ZCommandBuffer cmd, ZPipelineLayout layout,
									 VkPipelineBindPoint bindingPoint)
{
	add_ref<std::vector<ZDescriptorSetLayout>> descriptorLayouts
		= layout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	if (uint32_t setCount = data_count(descriptorLayouts); setCount)
	{
		std::vector<VkDescriptorSet> sets(setCount, VK_NULL_HANDLE);
		std::transform(descriptorLayouts.begin(), descriptorLayouts.end(), sets.begin(),
			[](const ZDescriptorSetLayout& dsl) { return *dsl.getParam<ZDescriptorSet>(); });

		add_cref<ZDeviceInterface> di = cmd.getParamRef<ZDevice>().getInterface();
		di.vkCmdBindDescriptorSets(*cmd,
			bindingPoint,
			*layout,
			0,			//firstSet
			setCount,
			sets.data(),
			0,			//dynamicOffsetCount
			nullptr);	//pDynamicOffsets
	}
}

void commandBufferBindPipeline (ZCommandBuffer cmd, ZPipeline pipeline, bool bindDescriptorSets)
{
	auto bindingPoint		= pipeline.getParam<VkPipelineBindPoint>();
	auto pipelineLayout		= pipeline.getParam<ZPipelineLayout>();

	vkCmdBindPipeline(*cmd, bindingPoint, *pipeline);

	if (bindDescriptorSets)
	{
		commandBufferBindDescriptorSets(cmd, pipelineLayout, bindingPoint);
	}
}

void commandBufferBindVertexBuffers (ZCommandBuffer cmd, add_cref<VertexInput> input,
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

void commandBufferSetVertexInputEXT (ZCommandBuffer cmd, add_cref<VertexInput> input)
{
	add_cref<ZDeviceInterface> di = cmd.getParamRef<ZDevice>().getInterface();

	ZVertexInput2EXT ext(input);
	di.vkCmdSetVertexInputEXT(*cmd,
							data_count(ext.bindingDescriptions),
							data_or_null(ext.bindingDescriptions),
							data_count(ext.attributeDescriptions),
							data_or_null(ext.attributeDescriptions));
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
	else { ASSERTFALSE("Unknown index type"); }
	vkCmdBindIndexBuffer(*cmd, *buffer, offset, indexType);
}

void commandBufferBindDescriptorBuffers (
	ZCommandBuffer					cmd,
	ZPipeline						pipeline,
	std::initializer_list<ZBuffer>	buffers)
{
	add_cref<ZDeviceInterface>			di = cmd.getParamRef<ZDevice>().getInterface();
	ZPipelineLayout						layout = pipeline.getParam<ZPipelineLayout>();
	const VkPipelineBindPoint			bindPoint = pipeline.getParam<VkPipelineBindPoint>();
	std::vector<VkDescriptorBufferBindingInfoEXT> infos(
							buffers.size(), makeVkStructT<VkDescriptorBufferBindingInfoEXT>());
	const std::vector<VkDeviceSize> offsets(buffers.size(), 0u);
	std::vector<uint32_t> indices(buffers.size());
	for (auto begin = buffers.begin(), b = begin; b != buffers.end(); ++b)
	{
		const uint32_t idx = uint32_t(std::distance(begin, b));
		infos[idx].address = bufferGetAddress(*b);
		infos[idx].usage = b->getParamRef<VkBufferCreateInfo>().usage;
		indices[idx] = idx;
	}
	di.vkCmdBindPipeline(*cmd, bindPoint, *pipeline);
	di.vkCmdBindDescriptorBuffersEXT(*cmd, uint32_t(buffers.size()), infos.data());
	di.vkCmdSetDescriptorBufferOffsetsEXT(*cmd, bindPoint, *layout, 0u, uint32_t(buffers.size()), 
											indices.data(), offsets.data());
}

static bool verifyPushConstants (
	std::size_t										count,
	add_cptr<std::size_t>							sizes,
	add_cptr<type_index_with_default>				types,
	add_cref<std::vector<VkPushConstantRange>>		ranges,
	add_cref<std::vector<type_index_with_default>>	pcTypes)
{
	if (ranges.size() == count)
	{
		uint32_t offset = 0u;

		for (std::size_t i = 0u; i < count; ++i)
		{
			add_cref<VkPushConstantRange> r(ranges.at(i));
			if (r.offset != offset || r.size != sizes[i] || pcTypes.at(i) != types[i])
				return false;
			offset = ROUNDUP(offset + r.size, 4u);
		}
		return true;
	}
	return false;
}

bool verifyPushConstants (
	ZPipelineLayout						layout,
	std::size_t							count,
	add_cptr<std::size_t>				sizes,
	add_cptr<type_index_with_default>	types)
{
	add_cref<std::vector<VkPushConstantRange>> ranges =
		layout.getParamRef<std::vector<VkPushConstantRange>>();
	add_cref<std::vector<type_index_with_default>> pcTypes =
		layout.getParamRef<std::vector<type_index_with_default>>();
	return verifyPushConstants(count, sizes, types, ranges, pcTypes);
}

bool verifyPushConstants (
	std::initializer_list<ZShaderObject>	shaders,
	std::size_t								count,
	add_cptr<std::size_t>					sizes,
	add_cptr<type_index_with_default>		types)
{
	if (shaders.size() == 0u && 0u != count)
	{
		// there is nothing to verify
		return false;
	}

	auto byOffset = [](add_cref<VkPushConstantRange> lhs, add_cref<VkPushConstantRange> rhs) -> bool
	{
		return (lhs.offset < rhs.offset);
	};
	auto equals = [](add_cref<VkPushConstantRange> lhs, add_cref<VkPushConstantRange> rhs) -> bool
	{
		return (std::memcmp(&lhs, &rhs, sizeof(VkPushConstantRange)) == 0);
	};

	for (auto beg = shaders.begin(), i = beg; i != shaders.end(); ++i)
	{
		for (auto j = std::next(i); j != shaders.end(); ++j)
		{
			std::vector<VkPushConstantRange> lhs(i->getParamRef<std::vector<VkPushConstantRange>>());
			std::vector<VkPushConstantRange> rhs(j->getParamRef<std::vector<VkPushConstantRange>>());

			if (lhs.size() != rhs.size() || rhs.size() != count)
				return false;
			else
			{
				std::sort(lhs.begin(), lhs.end(), byOffset);
				std::sort(rhs.begin(), rhs.end(), byOffset);

				if (!std::equal(lhs.begin(), lhs.end(), rhs.begin(), equals)) return false;
			}
		}
	}

	for (auto i = shaders.begin(); i != shaders.end(); ++i)
	{
		if (false == verifyPushConstants(count, sizes, types,
							i->getParamRef<std::vector<VkPushConstantRange>>(),
							i->getParamRef<std::vector<type_index_with_default>>()))
		{
			return false;
		}
	}

	return true;
}

static void commandBufferPushConstants (
	ZPipelineLayout		layout,
	ZCommandBuffer		cmd,
	std::size_t			count,
	add_cptr<add_cptr<void>>	pValues,
	add_cref<std::vector<VkPushConstantRange>>	ranges)
{
	if (ranges.size() == count)
	{
		for (uint32_t i = 0u; i < data_count(ranges); ++i)
		{
			add_cref<VkPushConstantRange> r(ranges.at(i));
#if 0
			uint32_t	v{};
			UVec2		v2;
			UVec3		v3;
			UVec4		v4;
			switch (r.size / 4)
			{
			case 1:
				std::memcpy(&v, pValues[i], r.size);
				break;
			case 2:
				std::memcpy(&v2, pValues[i], r.size);
				break;
			case 3:
				std::memcpy(&v3, pValues[i], r.size);
				break;
			case 4:
				std::memcpy(&v4, pValues[i], r.size);
				break;
			}
#endif
			vkCmdPushConstants(*cmd, *layout, r.stageFlags, r.offset, r.size, pValues[i]);
		}
	}
}

void commandBufferPushConstants (
	ZCommandBuffer				cmd,
	ZPipelineLayout				layout,
	std::size_t					count,
	add_cptr<add_cptr<void>>	pValues)
{
	commandBufferPushConstants(layout, cmd, count, pValues,
								layout.getParamRef<std::vector<VkPushConstantRange>>());
}

void commandBufferPushConstants (
	ZCommandBuffer							cmd,
	ZPipelineLayout							layout,
	std::initializer_list<ZShaderObject>	shaders,
	std::size_t								count,
	add_cptr<add_cptr<void>>				pValues)
{
	if (auto shader = shaders.begin(); shader != shaders.end())
	{
		commandBufferPushConstants(layout, cmd, count, pValues,
			shader->getParamRef<std::vector<VkPushConstantRange>>());
	}
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

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer,
												   uint32_t subpass, VkSubpassContents contents)
{
	const uint32_t	countOfAttachments	= renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>();
	const uint32_t	subpassCount		= renderPass.getParam<ZDistType<SubpassCount, uint32_t>>();
	const bool differs = framebuffer.getParamRef<std::vector<ZImageView>>().size() != countOfAttachments;
	ASSERTMSG(!differs, "Attachment count from renderPas and framebuffer differs");
	ASSERTMSG(subpass < subpassCount, "Subpass index exceeds renderPass subpasses amount");
	ZRenderPassBeginInfo	renderPassBegin(cmd, renderPass, framebuffer, subpass, contents);
	VkRenderPassBeginInfo	info = renderPassBegin();
	vkCmdBeginRenderPass(*cmd, &info, contents);
	for (uint32_t i = 0; i < subpass; ++i)
	{
		vkCmdNextSubpass(*cmd, contents);
	}
	return renderPassBegin;
}

ZRenderPassBeginInfo commandBufferBeginRenderPass (ZCommandBuffer cmd, ZFramebuffer framebuffer,
												   uint32_t subpass, VkSubpassContents contents)
{
	return commandBufferBeginRenderPass(cmd, framebuffer.getParam<ZRenderPass>(), framebuffer, subpass, contents);
}

bool commandBufferNextSubpass (add_ref<ZRenderPassBeginInfo> beginInfo)
{
	const uint32_t subpassCount = beginInfo.getRenderPass().getParam<ZDistType<SubpassCount, uint32_t>>();
	if (beginInfo.getSubpass() < subpassCount)
	{
		vkCmdNextSubpass(*beginInfo.getCommandBuffer(), beginInfo.getContents());
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
		vkCmdNextSubpass(*cmdBuffer, beginInfo.getContents());
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

	add_cref<ZDeviceInterface> di = cmd.getParamRef<ZDevice>().getInterface();
	di.vkCmdBeginRendering(*cmd, &rInfo);
}

void commandBufferEndRendering (ZCommandBuffer cmd)

{
	cmd.getParamRef<ZDevice>().getInterface().vkCmdEndRendering(*cmd);
}

void commandBufferSetViewport (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain)
{
	vkCmdSetViewport(*cmd, 0u, 1u, &swapchain.viewport);
}

void commandBufferSetScissor (ZCommandBuffer cmd, add_cref<Canvas::Swapchain> swapchain)
{
	vkCmdSetScissor(*cmd, 0u, 1u, &swapchain.scissor);
}

void commandBufferSetDefaultDynamicStates (
	ZCommandBuffer			cmdBuffer,
	add_cref<VertexInput>	vertexInput,
	add_cref<VkViewport>	viewport,
	add_cptr<VkRect2D>		pScissor)
{
	add_cref<ZDeviceInterface> di = cmdBuffer.getParamRef<ZDevice>().getInterface();

	// VkPipelineVertexInputStateCreateInfo
	commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
	commandBufferSetVertexInputEXT(cmdBuffer, vertexInput);

	// VkPipelineInputAssemblyStateCreateInfo
	di.vkCmdSetPrimitiveTopologyEXT(*cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	di.vkCmdSetPrimitiveRestartEnableEXT(*cmdBuffer, VK_FALSE);

	// VkPipelineViewportStateCreateInfo
	di.vkCmdSetViewportWithCountEXT(*cmdBuffer, 1u, &viewport);
	const VkRect2D scissor = pScissor
		? *pScissor
		: makeRect2D(uint32_t(viewport.width), uint32_t(viewport.height), int32_t(viewport.x), int32_t(viewport.y));
	di.vkCmdSetScissorWithCountEXT(*cmdBuffer, 1u, &scissor);

	// VkPipelineRasterizationStateCreateInfo
	di.vkCmdSetDepthClampEnableEXT(*cmdBuffer, VK_FALSE);
	di.vkCmdSetRasterizerDiscardEnableEXT(*cmdBuffer, VK_FALSE);
	di.vkCmdSetPolygonModeEXT(*cmdBuffer, VK_POLYGON_MODE_FILL);
	di.vkCmdSetCullModeEXT(*cmdBuffer, VK_CULL_MODE_BACK_BIT);
	di.vkCmdSetFrontFaceEXT(*cmdBuffer, VK_FRONT_FACE_CLOCKWISE);
	di.vkCmdSetDepthBiasEnableEXT(*cmdBuffer, VK_FALSE);

	// VkPipelineDepthStencilStateCreateInfo
	di.vkCmdSetDepthTestEnableEXT(*cmdBuffer, VK_FALSE);
	di.vkCmdSetDepthWriteEnableEXT(*cmdBuffer, VK_FALSE);
	di.vkCmdSetStencilTestEnableEXT(*cmdBuffer, VK_FALSE);

	// VkPipelineMultisampleStateCreateInfo
	di.vkCmdSetRasterizationSamplesEXT(*cmdBuffer, VK_SAMPLE_COUNT_1_BIT);
	di.vkCmdSetSampleMaskEXT(*cmdBuffer, VK_SAMPLE_COUNT_1_BIT, makeQuickPtr(~0u));
	di.vkCmdSetAlphaToCoverageEnableEXT(*cmdBuffer, VK_FALSE);

	// VkPipelineColorBlendAttachmentState
	di.vkCmdSetColorBlendEnableEXT(*cmdBuffer, 0u, 1u, makeQuickPtr(gpp::defaultBlendAttachmentState.blendEnable));
	di.vkCmdSetColorBlendEquationEXT(*cmdBuffer, 0u, 1u, makeQuickPtr(makeColorBlendEquationExt(gpp::defaultBlendAttachmentState)));
	di.vkCmdSetColorWriteMaskEXT(*cmdBuffer, 0u, 1u, makeQuickPtr(gpp::defaultBlendAttachmentColorWriteMask));

	di.vkCmdSetConservativeRasterizationModeEXT(*cmdBuffer, VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);
	di.vkCmdSetSampleLocationsEnableEXT(*cmdBuffer, VK_FALSE);
	di.vkCmdSetProvokingVertexModeEXT(*cmdBuffer, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);

	di.vkCmdSetTessellationDomainOriginEXT(*cmdBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
}

void commandBufferDrawIndirect (ZCommandBuffer cmd, ZBuffer buffer)
{
	const type_index_with_default type = buffer.getParam<type_index_with_default>();
	ASSERTMSG((type == type_index_with_default::make<VkDrawIndirectCommand>()
			   || type == type_index_with_default::make<VkDrawIndirectCommand[]>()),
			  "Buffer must be of type VkDrawIndirectCommand or VkDrawIndirectCommand[]");
	ASSERTMSG((buffer.getParamRef<VkBufferCreateInfo>().usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
			  "Buffer must created with usage bit VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT set");
	const VkDeviceSize size	= buffer.getParam<VkDeviceSize>();
	vkCmdDrawIndirect(*cmd, *buffer, 0u, uint32_t(size / sizeof(VkDrawIndirectCommand)),
										uint32_t(sizeof(VkDrawIndirectCommand)));
}

void commandBufferDrawIndexedIndirect (ZCommandBuffer cmd, ZBuffer buffer)
{
	const type_index_with_default type = buffer.getParam<type_index_with_default>();
	ASSERTMSG((type == type_index_with_default::make<VkDrawIndexedIndirectCommand>()
			   || type == type_index_with_default::make<VkDrawIndexedIndirectCommand[]>()),
			  "Buffer must be of type VkDrawIndexedIndirectCommand or VkDrawIndexedIndirectCommand[]");
	ASSERTMSG((buffer.getParamRef<VkBufferCreateInfo>().usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
			  "Buffer must created with usage bit VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT set");
	const VkDeviceSize size	= buffer.getParam<VkDeviceSize>();
	vkCmdDrawIndexedIndirect(*cmd, *buffer, 0u, uint32_t(size / sizeof(VkDrawIndexedIndirectCommand)),
												uint32_t(sizeof(VkDrawIndexedIndirectCommand)));
}

void commandBufferResetQueryPool (ZCommandBuffer cmd, ZQueryPool queryPool)
{
	const uint32_t queryCount = queryPool.getParam<uint32_t>();
	vkCmdResetQueryPool(*cmd, *queryPool, 0u, queryCount);
}

ZQueryPoolBeginInfo commandBufferBeginQuery (ZCommandBuffer cmd, ZQueryPool pool, uint32_t query, VkQueryControlFlags flags)
{
	vkCmdBeginQuery(*cmd, *pool, query, flags);
	return { cmd, pool, query };
}

void commandBufferEndQuery (add_cref<ZQueryPoolBeginInfo> queryPoolBeginInfo)
{
	vkCmdEndQuery(*queryPoolBeginInfo.cmd, *queryPoolBeginInfo.pool, queryPoolBeginInfo.query);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image, add_cref<VkClearColorValue> clearValue, VkImageLayout finalLayout)
{
	commandBufferClearColorImage(cmd, image, clearValue, imageMakeSubresourceRange(image), finalLayout);
}

void commandBufferClearColorImage (ZCommandBuffer cmd, ZImage image,
								   add_cref<VkClearColorValue> clearValue,
								   add_cref<VkImageSubresourceRange> range,
								   VkImageLayout finalLayout)
{
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
