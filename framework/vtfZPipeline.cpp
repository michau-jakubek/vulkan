#include "vtfZPipeline.hpp"
#include "vtfStructUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZImage.hpp"

#include <array>
#include <iostream>
#include <numeric>

namespace vtf
{

VkPipelineInputAssemblyStateCreateInfo  makeInputAssemblyStateCreateInfo ()
{
	VkPipelineInputAssemblyStateCreateInfo assemblyState = makeVkStruct();
	assemblyState.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	assemblyState.primitiveRestartEnable = VK_FALSE;
	return assemblyState;
}

VkPipelineVertexInputStateCreateInfo makeVertexInputStateCreateInfo ()
{
	VkPipelineVertexInputStateCreateInfo vertexInputState = makeVkStruct();
	return vertexInputState;
}

VkPipelineTessellationStateCreateInfo makeTessellationStateCreateInfo ()
{
	VkPipelineTessellationStateCreateInfo tessellationState = makeVkStruct();
	tessellationState.flags = 0;
	tessellationState.patchControlPoints = 0;
	return tessellationState;
}

VkPipelineViewportStateCreateInfo makeViewportStateCreateInfo ()
{
	VkPipelineViewportStateCreateInfo	viewportState = makeVkStruct();
	viewportState.viewportCount	= 1u;
	viewportState.scissorCount	= 1u;
	return viewportState;
}

VkPipelineRasterizationStateCreateInfo makeRasterizationCreateInfo ()
{
	VkPipelineRasterizationStateCreateInfo rasterizationState = makeVkStruct();
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
	VkPipelineMultisampleStateCreateInfo multisampleState = makeVkStruct();
	multisampleState.sampleShadingEnable	= VK_FALSE;
	multisampleState.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;
	return multisampleState;
}

VkPipelineDepthStencilStateCreateInfo makeDepthStencilStateCreateInfo ()
{
	VkPipelineDepthStencilStateCreateInfo	depthStencilState = makeVkStruct();
	depthStencilState.depthTestEnable		= VK_FALSE;
	depthStencilState.depthWriteEnable		= VK_FALSE;
	depthStencilState.depthCompareOp		= VK_COMPARE_OP_LESS;
	depthStencilState.depthBoundsTestEnable	= VK_FALSE;
	depthStencilState.minDepthBounds		= 0.0f;
	depthStencilState.maxDepthBounds		= 1.0f;
	depthStencilState.stencilTestEnable		= VK_FALSE;
	return depthStencilState;
}

VkPipelineColorBlendStateCreateInfo makeBlendStateCreateInfo ()
{
	VkPipelineColorBlendStateCreateInfo colorBlending = makeVkStruct();
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
	return makeVkStructT<VkPipelineDynamicStateCreateInfo>();
}

VkGraphicsPipelineCreateInfo makePipelineCreateInfo ()
{
	return makeVkStructT<VkGraphicsPipelineCreateInfo>();
}

typedef std::map<VkShaderStageFlagBits, add_ptr<ZSpecializationInfo>> SpecializationInfoPerStageMap;

struct GraphicPipelineSettings
{
	ZPipelineLayout		m_layout;
	ZRenderPass			m_renderPass;
	ZPipelineCache		m_pipelineCache;

	SpecializationInfoPerStageMap						m_shaderPerStageSpecs;
	std::vector<VkPipelineShaderStageCreateInfo>		m_shaderStages;
	std::vector<VkSpecializationInfo>					m_shaderSpecs;
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

	std::vector<gpp::AttachmentLocation>				m_drAttachments;
	uint32_t											m_drViewMask;

	GraphicPipelineSettings (ZPipelineLayout layout);
	auto findShader (VkShaderStageFlagBits stage) -> std::vector<VkPipelineShaderStageCreateInfo>::iterator;
	uint32_t containsDynamicState (VkDynamicState dynamicState) const;
	void updateShaderSpecs ();
};

GraphicPipelineSettings::GraphicPipelineSettings (ZPipelineLayout layout)
	: m_layout				(layout)
	, m_renderPass			()
	, m_pipelineCache		()
	, m_shaderPerStageSpecs	()
	, m_shaderStages		()
	, m_shaderSpecs			()
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
	, m_blendAttachments	()
	, m_blendState			(makeBlendStateCreateInfo())
	, m_dynamicStates		()
	, m_dynamicState		(makeDynamicStateCreateInfo())
	, m_createInfo			(makePipelineCreateInfo())
	, m_drAttachments		()
	, m_drViewMask			(0)
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

void GraphicPipelineSettings::updateShaderSpecs ()
{
	uint32_t stageIndex = 0u;
	m_shaderSpecs.resize(m_shaderStages.size());
	for (add_ref<VkPipelineShaderStageCreateInfo> stage : m_shaderStages)
	{
		auto spec = m_shaderPerStageSpecs.find(stage.stage);
		if (spec != m_shaderPerStageSpecs.end() && spec->second != nullptr && !spec->second->empty())
		{
			m_shaderSpecs.at(stageIndex) = spec->second->operator()();
			stage.pSpecializationInfo = &m_shaderSpecs.data()[stageIndex];
			stageIndex = stageIndex + 1u;
		}
		else
		{
			stage.pSpecializationInfo = nullptr;
		}
	}
}
std::shared_ptr<GraphicPipelineSettings> makeGraphicsPipelineSettings (ZPipelineLayout layout)
{
	return std::make_shared<GraphicPipelineSettings>(layout);
}

ZPipeline createGraphicsPipeline (GraphicPipelineSettings& settings)
{
	VkGraphicsPipelineCreateInfo& info = settings.m_createInfo;

	settings.m_vertexInputState	= settings.m_zVertexInputState();

	settings.updateShaderSpecs();
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

	auto calcColorAttachmentCount = [](add_cref<std::vector<gpp::AttachmentLocation>> drAttachments) -> uint32_t
	{
		uint32_t count = 0u;
		for (add_cref<gpp::AttachmentLocation> l : drAttachments)
		{
			if (std::get<bool>(l)) ++count;
		}
		return count;
	};
	const uint32_t colorAttachmentCount =
		settings.m_renderPass.has_handle()
			? settings.m_renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>().get()
			: calcColorAttachmentCount(settings.m_drAttachments);
	ASSERTMSG(colorAttachmentCount != 0u, "There must be at least one attachment");
	if (settings.m_blendAttachments.empty())
	{
		settings.m_blendAttachments.resize(colorAttachmentCount, gpp::defaultBlendAttachmentState);
	}
	settings.m_blendState.attachmentCount	= data_count(settings.m_blendAttachments);
	settings.m_blendState.pAttachments		= settings.m_blendAttachments.data();
	info.pColorBlendState					= &settings.m_blendState;

	if (settings.m_dynamicState.dynamicStateCount)
	{
		settings.m_dynamicState.pDynamicStates	= settings.m_dynamicStates.data();
		info.pDynamicState						= &settings.m_dynamicState;
	}

	if (settings.m_layout.getParam<bool>(/*enableDescriptorBuffer*/))
	{
		info.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	}

	void_ptr infoPNext = nullptr;
	std::vector<VkFormat> drFormats;
	std::vector<uint32_t> drIndices;
	std::vector<uint32_t> drLocations;
	VkRenderingAttachmentLocationInfo rali = makeVkStruct();
	VkRenderingInputAttachmentIndexInfo riai = makeVkStruct();
	VkPipelineRenderingCreateInfo drInfo = makeVkStruct(&rali);
	
	if (false == settings.m_renderPass.has_handle())
	{
		drLocations.resize(colorAttachmentCount);
		rali.colorAttachmentCount = colorAttachmentCount;
		rali.pColorAttachmentLocations = drLocations.data();

		drFormats.resize(colorAttachmentCount, VK_FORMAT_UNDEFINED);
		for (uint32_t i = 0u, k = 0u; i < data_count(settings.m_drAttachments); ++i)
		{
			add_cref<gpp::AttachmentLocation> l(settings.m_drAttachments[i]);
			const uint32_t index = std::get<uint32_t>(l);
			if (std::get<bool>(l))
			{
				drLocations[k] = index;
				ZImageView a = std::get<ZImageView>(l);
				if (a.has_handle())
				{
					drFormats[k] = imageGetFormat(imageViewGetImage(a));
				}
				k = k + 1u;
			}
			else
			{
				drIndices.push_back(index);
			}
		}
		if (drIndices.empty() == false)
		{
			riai.colorAttachmentCount = data_count(drIndices);
			riai.pColorAttachmentInputIndices = drIndices.data();
			rali.pNext = &riai;
		}

		infoPNext = &drInfo;
		drInfo.viewMask					= settings.m_drViewMask;
		drInfo.colorAttachmentCount		= colorAttachmentCount;
		drInfo.pColorAttachmentFormats	= drFormats.data();
		drInfo.depthAttachmentFormat	= VK_FORMAT_UNDEFINED;
		drInfo.stencilAttachmentFormat	= VK_FORMAT_UNDEFINED;
	}

	info.pNext				= infoPNext;
	info.layout				= *settings.m_layout;
	info.renderPass			= settings.m_renderPass.has_handle() ? *settings.m_renderPass : VK_NULL_HANDLE;
	info.basePipelineHandle	= VK_NULL_HANDLE;
	info.basePipelineIndex	= -1;

	VkPipeline					pipelineHandle	= VK_NULL_HANDLE;
	VkPipelineCache				pipelineCache	= settings.m_pipelineCache.has_handle()
													? *settings.m_pipelineCache
													: VkPipelineCache(VK_NULL_HANDLE);
	ZDevice						device			= settings.m_layout.getParam<ZDevice>();
	auto						callbacks		= settings.m_layout.getParam<VkAllocationCallbacksPtr>();
	const VkResult				createStatus	= vkCreateGraphicsPipelines	(*device,
																			 pipelineCache,
																			 1u, &info,
																			 callbacks,
																			 &pipelineHandle);
	VKASSERT(createStatus);

	return ZPipeline::create(pipelineHandle, device, callbacks, settings.m_layout,
							 settings.m_renderPass, VK_PIPELINE_BIND_POINT_GRAPHICS, info.flags);
}

void updateSettings (add_ref<GraphicPipelineSettings>) { /* end of template recursion */ }

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, ZPipelineCreateFlags createFlags)
{
	settings.m_createInfo.flags = createFlags();
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, ZShaderModule shaderModule)
{
	if (false == shaderModule.has_handle()) return;
	const VkShaderStageFlagBits stage = shaderModule.getParam<VkShaderStageFlagBits>();
	auto i = settings.findShader(stage);
	if (i != settings.m_shaderStages.end())
	{
		i->module	= *shaderModule;
		i->pName	= shaderModule.getParamRef<std::string>().c_str();
	}
	else
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage	= stage;
		shaderStageInfo.module	= *shaderModule;
		shaderStageInfo.pName	= shaderModule.getParamRef<std::string>().c_str();
		settings.m_shaderStages.emplace_back(shaderStageInfo);
	}
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::BlendAttachmentState> blendAttachmentState)
{
	const uint32_t attachment = blendAttachmentState.get().first;
	settings.m_blendAttachments.resize((attachment + 1u), gpp::defaultBlendAttachmentState);
	settings.m_blendAttachments.at(attachment) = blendAttachmentState.get().second;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::BlendConstants> blendConstants)
{
	settings.m_blendState.blendConstants[0] = blendConstants.get()[0];
	settings.m_blendState.blendConstants[1] = blendConstants.get()[1];
	settings.m_blendState.blendConstants[2] = blendConstants.get()[2];
	settings.m_blendState.blendConstants[3] = blendConstants.get()[3];
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::SpecConstants> specConstants)
{
	settings.m_shaderPerStageSpecs[specConstants.get().first] = &specConstants.get().second;
}

void updateKnownSettings(add_ref<GraphicPipelineSettings> settings, add_cref<gpp::PrimitiveRestart> primitiveRestartEnable)
{
	settings.m_assemblyState.primitiveRestartEnable = primitiveRestartEnable ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, ZRenderPass renderPass)
{
	settings.m_renderPass = renderPass;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, ZPipelineCache pipelineCache)
{
	settings.m_pipelineCache = pipelineCache;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::AttachmentLocation> drAttachment)
{
	settings.m_drAttachments.push_back(drAttachment);
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::ViewMask> drViewMask)
{
	settings.m_drViewMask = drViewMask;
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

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::DepthTestEnable> enableDepthTest)
{
	settings.m_depthStencilState.depthTestEnable = enableDepthTest ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::DepthWriteEnable> enableDepthWrite)
{
	settings.m_depthStencilState.depthWriteEnable = enableDepthWrite ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::StencilTestEnable> enableStencilTest)
{
	settings.m_depthStencilState.stencilTestEnable = enableStencilTest ? VK_TRUE : VK_FALSE;
}

void updateKnownSettings (add_ref<GraphicPipelineSettings> settings, add_cref<gpp::SubpassIndex> subpassIndex)
{
	settings.m_createInfo.subpass = subpassIndex;
}

bool computePipelineVerifyLimits (ZDevice device, add_cref<UVec3> wgSizes, bool raise)
{
	bool result = true;
	uint64_t product = 1u;
	add_cref<VkPhysicalDeviceLimits> limits = deviceGetPhysicalLimits(device);

	for (uint32_t i = 0; result && i < 4; ++i)
	{
		if (i < 3)
		{
			product *= make_unsigned(wgSizes[i]);
			if (make_unsigned(wgSizes[i]) > limits.maxComputeWorkGroupSize[i])
			{
				result = false;
				if (raise)
				{
					ASSERTFALSE("workGroupSize[", i, "] of ", wgSizes[i],
						" is greater than available ", limits.maxComputeWorkGroupSize[i]);
				}
			}
			/*
			 * Validation Error: [ VUID-RuntimeSpirv-x-06432 ] type = VK_OBJECT_TYPE_SHADER_MODULE;
			 * local_size (32, 32, 32) exceeds device limit maxComputeWorkGroupInvocations (1536).
			 * The Vulkan spec states: The product of x size, y size, and z size in LocalSize or LocalSizeId
			 * must be less than or equal to VkPhysicalDeviceLimits::maxComputeWorkGroupInvocations
			 * (https://vulkan.lunarg.com/doc/view/1.3.243.0/linux/1.3-extensions/vkspec.html#VUID-RuntimeSpirv-x-06432)
			 */
			 /*
			  * uint32_t maxComputeWorkGroupCount[3];	is the maximum number of local workgroups that can be dispatched by a single dispatching command
			  * uint32_t maxComputeWorkGroupInvocations;	is the maximum total number of compute shader invocations in a single local workgroup. The product of the X, Y, and Z sizes.
			  * uint32_t maxComputeWorkGroupSize[3];		is the maximum size of a local compute workgroup, per dimension.
			  */
			if (i == 3 && product > limits.maxComputeWorkGroupInvocations)
			{
				result = false;
				if (raise)
				{
					ASSERTFALSE("localSize product of ", product, " is grater than available ", limits.maxComputeWorkGroupInvocations);
				}
			}
		}
	}

	return result;
}

ZPipeline createComputePipelineImpl (
	ZPipelineLayout					layout,
	ZShaderModule					computeShaderModule,
	ZPipelineCache					pipelineCache,
	add_cref<UVec3>					localSizes,
	bool							autoLocalSizesIDs,
	bool							enableFullGroups,
	add_ref<ZSpecializationInfo>	info)
{
	ZDevice									aDevice		= layout.getParam<ZDevice>();
	const VkAllocationCallbacksPtr			callbacks	= aDevice.getParam<VkAllocationCallbacksPtr>();
	ZPhysicalDevice							aPhysDevice	= aDevice.getParam<ZPhysicalDevice>();

	UNREF(enableFullGroups);
	/*
	VkPhysicalDeviceSubgroupSizeControlFeatures			sizeCtrlFeatures = makeVkStruct();
	deviceGetPhysicalFeatures2(aPhysDevice, &sizeCtrlFeatures);

	VkPhysicalDeviceSubgroupProperties					subgroupProperties = makeVkStruct();

	VkPipelineShaderStageRequiredSubgroupSizeCreateInfo	subgroupCreateInfo = makeVkStruct();
	*/

	if (autoLocalSizesIDs) computePipelineVerifyLimits(aDevice, localSizes, true);

	const VkSpecializationInfo specInfo = info();

	VkPipelineShaderStageCreateInfo	sci = makeVkStruct();
	sci.flags	= VkPipelineShaderStageCreateFlags(0);
	sci.stage	= VK_SHADER_STAGE_COMPUTE_BIT;
	sci.module	= *computeShaderModule;
	sci.pName	= computeShaderModule.getParamRef<std::string>().c_str();
	sci.pSpecializationInfo	= info.empty() ? nullptr : &specInfo;

	const VkPipelineCreateFlags ciFlags = layout.getParam<bool>(/*enableDescriptorBuffer*/)
											? VkPipelineCreateFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
											: VkPipelineCreateFlags(0);
	VkComputePipelineCreateInfo	ci = makeVkStruct();
	ci.flags	= ciFlags;
	ci.stage	= sci;
	ci.layout	= *layout;
	ci.basePipelineHandle	= VK_NULL_HANDLE;
	ci.basePipelineIndex	= 0;

	VkPipelineCache cache = pipelineCache.has_handle() ? *pipelineCache : VK_NULL_HANDLE;
	ZPipeline	computePipeline (VK_NULL_HANDLE, aDevice, callbacks, layout, ZRenderPass(),
								 VK_PIPELINE_BIND_POINT_COMPUTE, ci.flags);
	VKASSERT(vkCreateComputePipelines(*aDevice, cache, 1u, &ci, callbacks, computePipeline.setter()));

	return computePipeline;
}

ZPipeline createComputePipeline (ZPipelineCache pipelineCache, ZPipelineLayout layout, ZShaderModule computeShaderModule,
								add_ref<ZSpecializationInfo> specInfo, bool enableFullGroups)
{
	return createComputePipelineImpl(layout, computeShaderModule, pipelineCache, UVec3(INVALID_UINT32), false, enableFullGroups, specInfo);
}

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline)
{
	return pipeline.getParam<ZPipelineLayout>();
}

ZPipelineCache createPipelineCache (ZDevice device, add_cref<std::string> cacheFileName,
									VkPipelineCacheCreateFlags flags, bool forceReset)
{
	VkPipelineCacheCreateInfo pcci{};
	pcci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	pcci.flags = flags;

	if (forceReset)
	{
		pcci.initialDataSize = 0u;
		pcci.pInitialData = nullptr;
	}
	else
	{
		std::vector<char> data;
		add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
		const fs::path tmpPath = std::strlen(gf.tmpDir) ? fs::path(gf.tmpDir) : fs::temp_directory_path();
		const fs::path cachePath = tmpPath / cacheFileName;
		if (const uint32_t size = readFile(cachePath, data); size != INVALID_UINT32)
		{
			pcci.initialDataSize = size_t(size);
			pcci.pInitialData = data.data();
		}
		else
		{
			pcci.initialDataSize = 0u;
			pcci.pInitialData = nullptr;
		}
	}

	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();
	ZPipelineCache	cache(VK_NULL_HANDLE, device, callbacks, cacheFileName);
	VKASSERT(vkCreatePipelineCache(*device, &pcci, callbacks, cache.setter()));

	return cache;
}

} // namespace vtf
