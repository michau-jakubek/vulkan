#include "vtfZPipeline.hpp"
#include <array>

namespace vtf
{

VkPipelineInputAssemblyStateCreateInfo  makeInputAssemblyStateCreateInfo ()
{
	VkPipelineInputAssemblyStateCreateInfo assemblyState{};
	assemblyState.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyState.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	assemblyState.primitiveRestartEnable = VK_FALSE;
	return assemblyState;
}

VkPipelineVertexInputStateCreateInfo makeVertexInputStateCreateInfo ()
{
	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	return vertexInputState;
}

VkPipelineTessellationStateCreateInfo makeTessellationStateCreateInfo ()
{
	VkPipelineTessellationStateCreateInfo tessellationState{};
	tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationState.pNext = nullptr;
	tessellationState.flags = 0;
	tessellationState.patchControlPoints = 0;
	return tessellationState;
}

VkPipelineViewportStateCreateInfo makeViewportStateCreateInfo ()
{
	VkPipelineViewportStateCreateInfo	viewportState{};
	viewportState.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount	= 1u;
	viewportState.scissorCount	= 1u;
	return viewportState;
}

VkPipelineRasterizationStateCreateInfo makeRasterizationCreateInfo ()
{
	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable			= VK_FALSE;
	rasterizationState.rasterizerDiscardEnable	= VK_FALSE;
	rasterizationState.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizationState.lineWidth				= 1.0f;
	rasterizationState.rasterizerDiscardEnable	= VK_FALSE;
	rasterizationState.cullMode					= VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace				= VK_FRONT_FACE_CLOCKWISE;
	rasterizationState.depthBiasEnable			= VK_FALSE;
	return rasterizationState;
}

VkPipelineMultisampleStateCreateInfo makeMultisampleStateCreateInfo ()
{
	VkPipelineMultisampleStateCreateInfo multisampleState{};
	multisampleState.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.sampleShadingEnable	= VK_FALSE;
	multisampleState.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
	return multisampleState;
}

VkPipelineDepthStencilStateCreateInfo makeDepthStencilStateCreateInfo ()
{
	VkPipelineDepthStencilStateCreateInfo	depthStencilState{};
	depthStencilState.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.pNext					= nullptr;
	depthStencilState.depthTestEnable		= VK_TRUE;
	depthStencilState.depthWriteEnable		= VK_TRUE;
	depthStencilState.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable	= VK_FALSE;
	depthStencilState.minDepthBounds		= 0.0f;
	depthStencilState.maxDepthBounds		= 1.0f;
	depthStencilState.stencilTestEnable		= VK_FALSE;
	return depthStencilState;
}

VkPipelineColorBlendAttachmentState makeBlendAttachmentState ()
{
	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT
										| VK_COLOR_COMPONENT_G_BIT
										| VK_COLOR_COMPONENT_B_BIT
										| VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.blendEnable	= VK_FALSE;
	return blendAttachmentState;
}

VkPipelineColorBlendStateCreateInfo makeBlendStateCreateInfo ()
{
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.logicOp			= VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount	= 0u;
	colorBlending.pAttachments		= nullptr;
	colorBlending.blendConstants[0]	= 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;
	return colorBlending;
}

VkPipelineDynamicStateCreateInfo makeDynamicStateCreateInfo ()
{
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	return dynamicState;
}

VkGraphicsPipelineCreateInfo makePipelineCreateInfo ()
{
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	return pipelineInfo;
}

struct GraphicPipelineSettings
{
	ZPipelineLayout		m_layout;
	ZRenderPass			m_renderPass;

	std::vector<VkPipelineShaderStageCreateInfo>		m_shaderStages;
	VkPipelineInputAssemblyStateCreateInfo				m_assemblyState;
	ZPipelineVertexInputStateCreateInfo					m_zVertexInputState;
	VkPipelineVertexInputStateCreateInfo				m_vertexInputState;
	VkPipelineTessellationStateCreateInfo				m_tessellationState;

	VkViewport											m_viewport;
	VkRect2D											m_scissor;
	VkPipelineViewportStateCreateInfo					m_viewportState;

	VkPipelineRasterizationStateCreateInfo				m_rasterizationState;
	VkPipelineMultisampleStateCreateInfo				m_multisampleState;
	VkPipelineDepthStencilStateCreateInfo				m_depthStencilState;

	std::vector<VkPipelineColorBlendAttachmentState>	m_blendAttachments;
	VkPipelineColorBlendStateCreateInfo					m_blendState;

	std::array<VkDynamicState, 55>						m_dynamicStates;
	VkPipelineDynamicStateCreateInfo					m_dynamicState;

	VkGraphicsPipelineCreateInfo						m_createInfo;

	GraphicPipelineSettings (ZPipelineLayout layout, ZRenderPass renderPass);
	auto findShader (VkShaderStageFlagBits stage) -> std::vector<VkPipelineShaderStageCreateInfo>::iterator;
	uint32_t containsDynamicState (VkDynamicState dynamicState) const;
};

GraphicPipelineSettings::GraphicPipelineSettings (ZPipelineLayout layout, ZRenderPass renderPass)
	: m_layout				(layout)
	, m_renderPass			(renderPass)
	, m_shaderStages		()
	, m_assemblyState		(makeInputAssemblyStateCreateInfo())
	, m_zVertexInputState	()
	, m_vertexInputState	(makeVertexInputStateCreateInfo())
	, m_tessellationState	(makeTessellationStateCreateInfo())
	, m_viewport			(makeViewport(0, 0))
	, m_scissor				(makeRect2D(0, 0))
	, m_viewportState		(makeViewportStateCreateInfo())
	, m_rasterizationState	(makeRasterizationCreateInfo())
	, m_multisampleState	(makeMultisampleStateCreateInfo())
	, m_depthStencilState	(makeDepthStencilStateCreateInfo())
	, m_blendAttachments	(renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>(), makeBlendAttachmentState())
	, m_blendState			(makeBlendStateCreateInfo())
	, m_dynamicStates		()
	, m_dynamicState		(makeDynamicStateCreateInfo())
	, m_createInfo			(makePipelineCreateInfo())
{
}

auto GraphicPipelineSettings::findShader (VkShaderStageFlagBits stage)
	-> std::vector<VkPipelineShaderStageCreateInfo>::iterator
{
	return std::find_if(m_shaderStages.begin(), m_shaderStages.end(),
						[&](const VkPipelineShaderStageCreateInfo& info){ return info.stage == stage; });
}

uint32_t GraphicPipelineSettings::containsDynamicState (VkDynamicState dynamicState) const
{
	uint32_t i = 0u;
	while (i < m_dynamicState.dynamicStateCount && m_dynamicStates[i] != dynamicState) { i = i + 1u; }
	return (i < m_dynamicState.dynamicStateCount)
			? (m_dynamicState.dynamicStateCount)
			  ? (i)
			  : INVALID_UINT32
			: INVALID_UINT32;
}

std::shared_ptr<GraphicPipelineSettings> makeGraphicsPipelineSettings (ZPipelineLayout layout, ZRenderPass renderPass)
{
	return std::make_shared<GraphicPipelineSettings>(layout, renderPass);
}

ZPipeline createGraphicsPipeline (GraphicPipelineSettings& settings)
{
	VkGraphicsPipelineCreateInfo& info = settings.m_createInfo;

	settings.m_vertexInputState	= settings.m_zVertexInputState();

	info.stageCount				= static_cast<uint32_t>(settings.m_shaderStages.size());
	info.pStages				= settings.m_shaderStages.data();
	info.pVertexInputState		= &settings.m_vertexInputState;
	info.pInputAssemblyState	= &settings.m_assemblyState;

	if ((settings.findShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != settings.m_shaderStages.end())
		|| (settings.findShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != settings.m_shaderStages.end()))
	{
		info.pTessellationState = &settings.m_tessellationState;
	}

	if (settings.containsDynamicState(VK_DYNAMIC_STATE_VIEWPORT) == INVALID_UINT32)
	{
		settings.m_viewportState.pViewports		= &settings.m_viewport;
	}
	if (settings.containsDynamicState(VK_DYNAMIC_STATE_SCISSOR) == INVALID_UINT32)
	{
		settings.m_viewportState.pScissors		= &settings.m_scissor;
		clampScissorToViewport(settings.m_viewport, settings.m_scissor);
	}
	info.pViewportState			= &settings.m_viewportState;

	info.pRasterizationState	= &settings.m_rasterizationState;
	info.pMultisampleState		= &settings.m_multisampleState;
	info.pDepthStencilState		= &settings.m_depthStencilState;

	const uint32_t attachmentCount = settings.m_renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>();
	ASSERTION(settings.m_blendAttachments.size() <= attachmentCount);
	settings.m_blendState.attachmentCount	= attachmentCount;
	settings.m_blendState.pAttachments		= settings.m_blendAttachments.data();
	info.pColorBlendState					= &settings.m_blendState;

	if (settings.m_dynamicState.dynamicStateCount)
	{
		settings.m_dynamicState.pDynamicStates	= settings.m_dynamicStates.data();
		info.pDynamicState						= &settings.m_dynamicState;
	}

	info.layout				= *settings.m_layout;
	info.renderPass			= *settings.m_renderPass;
	info.basePipelineHandle	= VK_NULL_HANDLE;
	info.basePipelineIndex	= 0u;

	VkPipeline					pipelineHandle	= VK_NULL_HANDLE;
	ZDevice						device			= settings.m_layout.getParam<ZDevice>();
	auto						callbacks		= settings.m_layout.getParam<VkAllocationCallbacksPtr>();
	const VkResult				createStatus	= vkCreateGraphicsPipelines	(*device,
																			 VkPipelineCache(0),
																			 1u, &info,
																			 callbacks,
																			 &pipelineHandle);
	VKASSERT2(createStatus);

	return ZPipeline::create(pipelineHandle, device, callbacks,
							 settings.m_layout, settings.m_renderPass, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void updateSettings (add_ref<GraphicPipelineSettings>) { /* end of template recursion */ }

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, ZShaderModule shaderModule)
{
	const VkShaderStageFlagBits stage = shaderModule.getParam<VkShaderStageFlagBits>();
	auto i = settings.findShader(stage);
	if (i != settings.m_shaderStages.end())
	{
		i->module = *shaderModule;
	}
	else
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage	= stage;
		shaderStageInfo.module	= *shaderModule;
		shaderStageInfo.pName	= "main";
		settings.m_shaderStages.emplace_back(shaderStageInfo);
	}
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<VertexBinding> vertexBinding)
{
	settings.m_zVertexInputState.swap(ZPipelineVertexInputStateCreateInfo(vertexBinding));
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<VertexInput> vertexInput)
{
	settings.m_zVertexInputState.swap(ZPipelineVertexInputStateCreateInfo(vertexInput));
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, VkPrimitiveTopology topology)
{
	settings.m_assemblyState.topology = topology;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::PatchControlPoints> patchControlPoints)
{
	settings.m_tessellationState.patchControlPoints = patchControlPoints;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, VkDynamicState dynamicState)
{
	if (settings.containsDynamicState(dynamicState) == INVALID_UINT32)
	{
		settings.m_dynamicStates[settings.m_dynamicState.dynamicStateCount++] = dynamicState;
	}
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::ViewportCount> viewportCount)
{
	settings.m_viewportState.viewportCount = viewportCount;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::ScissorCount> scissorCount)
{
	settings.m_viewportState.scissorCount = scissorCount;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, VkPolygonMode polygonMode)
{
	settings.m_rasterizationState.polygonMode = polygonMode;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::LineWidth> lineWidth)
{
	settings.m_rasterizationState.lineWidth = lineWidth;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::CullModeFlags> cullModeFlags)
{
	settings.m_rasterizationState.cullMode = cullModeFlags;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, VkFrontFace frontFace)
{
	settings.m_rasterizationState.frontFace = frontFace;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, VkSampleCountFlagBits sampleCount)
{
	settings.m_multisampleState.rasterizationSamples = sampleCount;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<VkExtent2D> extent)
{
	settings.m_viewport = makeViewport(extent.width, extent.height);
	settings.m_scissor	= makeRect2D(extent.width, extent.height);
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<VkRect2D> scissor)
{
	settings.m_scissor = scissor;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<VkViewport> vieport)
{
	settings.m_viewport = vieport;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::DepthTestEnable> depthTestEnable)
{
	settings.m_depthStencilState.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::DepthWriteEnable> depthTestMask)
{
	settings.m_depthStencilState.depthWriteEnable = depthTestMask ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::StencilTestEnable> stencilTestEnable)
{
	settings.m_depthStencilState.stencilTestEnable = stencilTestEnable ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::SubpassIndex> subpassIndex)
{
	settings.m_createInfo.subpass = subpassIndex;
}

ZPipeline createComputePipeline (ZPipelineLayout layout, ZShaderModule computeShaderModule,
								 add_cref<UVec3> workGroupSize, add_cref<UVec3> specID, bool enableFullGroups)
{
	ZDevice									aDevice		= layout.getParam<ZDevice>();
	const VkAllocationCallbacksPtr			callbacks	= aDevice.getParam<VkAllocationCallbacksPtr>();
	ZPhysicalDevice							aPhysDevice	= aDevice.getParam<ZPhysicalDevice>();
	add_cref<VkPhysicalDeviceProperties>	properties	= aPhysDevice.getParamRef<VkPhysicalDeviceProperties>();

	VkPhysicalDeviceSubgroupSizeControlFeatures			sizeCtrlFeatures{};
	sizeCtrlFeatures.sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;
	sizeCtrlFeatures.pNext				= nullptr;

	VkPhysicalDeviceFeatures2							features2{};
	features2.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &sizeCtrlFeatures;

	VkPhysicalDeviceSubgroupProperties					subgroupProperties{};
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

	VkPhysicalDeviceProperties2							physicalDeviceProperties2{};
	physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties2.pNext = &subgroupProperties;

	VkPipelineShaderStageRequiredSubgroupSizeCreateInfo	subgroupCreateInfo{};
	subgroupCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

	VkPipelineShaderStageCreateFlags	shaderStageCreateFlags	= 0;
	void*								shaderStagePNext		= nullptr;

	if (enableFullGroups)
	{
		vkGetPhysicalDeviceFeatures2(*aPhysDevice, &features2);
		if (sizeCtrlFeatures.computeFullSubgroups)
		{
			vkGetPhysicalDeviceProperties2(*aPhysDevice, &physicalDeviceProperties2);
			subgroupCreateInfo.requiredSubgroupSize = subgroupProperties.subgroupSize;
			shaderStageCreateFlags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
			shaderStagePNext = &subgroupCreateInfo;
		}
	}

	UVec3 workGroupSizeLimit;
	uint32_t maxComputeWorkGroupInvocations = 0;
	{
		workGroupSizeLimit.x(properties.limits.maxComputeWorkGroupSize[0]);
		workGroupSizeLimit.y(properties.limits.maxComputeWorkGroupSize[1]);
		workGroupSizeLimit.z(properties.limits.maxComputeWorkGroupSize[2]);
		maxComputeWorkGroupInvocations = properties.limits.maxComputeWorkGroupInvocations;
	}

	const VkSpecializationMapEntry entryTemplates[3]
	{
		{ specID[0], (uint32_t)(sizeof(uint32_t) * 0), sizeof(uint32_t) },
		{ specID[1], (uint32_t)(sizeof(uint32_t) * 1), sizeof(uint32_t) },
		{ specID[2], (uint32_t)(sizeof(uint32_t) * 2), sizeof(uint32_t) }
	};

	VkSpecializationMapEntry	entries[3];
	uint32_t					specData[3];
	uint32_t					entryCount	= 0u;
	uint32_t					product		= 1u;

	for (size_t i = 0; i < 3; ++i)
	{
		if (workGroupSize[i] != INVALID_UINT32)
		{
			ASSERTMSG(specID[i] != INVALID_UINT32, "invalid SpecID");
			entries[entryCount]		= entryTemplates[i];
			specData[entryCount]	= workGroupSize[i];
			product					*= workGroupSize[i];
			if (workGroupSize[i] > workGroupSizeLimit[i])
			{
				std::stringstream ss;
				ss << "workGroupSize[" << i << "] of " << workGroupSize[i]
					  << " is greaten than available " << workGroupSizeLimit;
				ASSERTMSG(false, ss.str());
			}
			entryCount += 1;
		}
	}

	/*
	 * Validation Error: [ VUID-RuntimeSpirv-x-06432 ] type = VK_OBJECT_TYPE_SHADER_MODULE;
	 * local_size (32, 32, 32) exceeds device limit maxComputeWorkGroupInvocations (1536).
	 * The Vulkan spec states: The product of x size, y size, and z size in LocalSize or LocalSizeId
	 * must be less than or equal to VkPhysicalDeviceLimits::maxComputeWorkGroupInvocations
	 * (https://vulkan.lunarg.com/doc/view/1.3.243.0/linux/1.3-extensions/vkspec.html#VUID-RuntimeSpirv-x-06432)
	 */
	if (product >  maxComputeWorkGroupInvocations)
	{
		std::stringstream ss;
		ss << "localSize product of " << product << " is grater than available " << maxComputeWorkGroupInvocations;
		ASSERTMSG(false, ss.str());
	}

	const VkSpecializationInfo specInfo
	{
		entryCount,										// mapEntryCount
		entries,										// pMapEntries
		(uint32_t)(entryCount * sizeof(specData[0])),	// dataSize
		&specData[0]									// pData
	};

	VkPipelineShaderStageCreateInfo	sci{};
	sci.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sci.pNext	= shaderStagePNext;
	sci.flags	= shaderStageCreateFlags;
	sci.stage	= VK_SHADER_STAGE_COMPUTE_BIT;
	sci.module	= *computeShaderModule;
	sci.pName	= "main";
	sci.pSpecializationInfo	= entryCount ? &specInfo : nullptr;

	VkComputePipelineCreateInfo	ci{};
	ci.sType	= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.pNext	= nullptr;
	ci.flags	= VkPipelineCreateFlags(0);
	ci.stage	= sci;
	ci.layout	= *layout;
	ci.basePipelineHandle	= VK_NULL_HANDLE;
	ci.basePipelineIndex	= 0;

	ZPipeline	computePipeline (VK_NULL_HANDLE, aDevice, callbacks, layout, ZRenderPass(), VK_PIPELINE_BIND_POINT_COMPUTE);
	VKASSERT2(vkCreateComputePipelines(*aDevice, VkPipelineCache(VK_NULL_HANDLE), 1u, &ci, callbacks, computePipeline.setter()));

	return computePipeline;
}

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline)
{
	return pipeline.getParam<ZPipelineLayout>();
}

} // namespace vtf