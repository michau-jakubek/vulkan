#include <array>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "vtfCUtils.hpp"
#include "vtfContext.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"

namespace vtf
{

std::map<VkQueueFlagBits, uint32_t> globalQueuesToIndices = { { VK_QUEUE_GRAPHICS_BIT, INVALID_UINT32 }, { VK_QUEUE_COMPUTE_BIT, INVALID_UINT32 } };

uint32_t VulkanContext::deviceIndex = INVALID_UINT32;

VulkanContext::VulkanContext (VkAllocationCallbacksPtr	allocationCallbacks,
							  VkDebugUtilsMessengerEXT	debugMessengerHandle,
							  VkDebugReportCallbackEXT	debugReportHandle,
							  ZInstance					anInstance,
							  ZPhysicalDevice			aPhysicalDevice,
							  ZDevice					aLogicalDevice)
	// begining references initialization
	: callbacks						(m_callbacks)
	, instance						(m_instance)
	, physicalDevice				(m_physicalDevice)
	, device						(m_device)
	, vertexInput					(m_vertexInput)
	// end of references initialization
	, m_callbacks					(allocationCallbacks)
	, m_debugMessenger				(debugMessengerHandle)
	, m_debugReport					(debugReportHandle)
	, m_instance					(anInstance)
	, m_physicalDevice				(aPhysicalDevice)
	, m_device						(aLogicalDevice)
	, m_vertexInput					(*this)
{
	VulkanContext::deviceIndex = aPhysicalDevice.getParam<uint32_t>();
}

VulkanContext::VulkanContext (const char*		appName,
							  const strings&	instanceLayers,
							  const strings&	instanceExtensions,
							  const strings&	deviceExtensions,
							  uint32_t			apiVersion,
							  uint32_t			engVersion,
							  uint32_t			appVersion)
	// begining references initialization
	: callbacks						(m_callbacks)
	, instance						(m_instance)
	, physicalDevice				(m_physicalDevice)
	, device						(m_device)
	, vertexInput					(m_vertexInput)
	// end of initialization of references
	, m_callbacks					(getAllocationCallbacks())
	, m_debugMessenger				(VK_NULL_HANDLE)
	, m_debugReport					(VK_NULL_HANDLE)
	, m_instance					(createInstance(appName, m_callbacks, instanceLayers, instanceExtensions, &m_debugMessenger, this, apiVersion, engVersion, appVersion))
	, m_physicalDevice				(selectPhysicalDevice(VulkanContext::deviceIndex, m_instance, deviceExtensions, globalQueuesToIndices))
	, m_device						(createLogicalDevice(m_physicalDevice, globalQueuesToIndices))
	, m_vertexInput					(*this)
{
}

VulkanContext::~VulkanContext ()
{
	if (m_debugMessenger != VK_NULL_HANDLE)
		destroyDebugMessenger(m_instance, m_callbacks, m_debugMessenger);
	if (m_debugReport != VK_NULL_HANDLE)
		destroyDebugReport(m_instance, m_callbacks, m_debugReport);
}

ZQueue VulkanContext::getGraphicsQueue() const
{
	add_cref<ZQueueInfos> queueInfos = device.getParamRef<ZQueueInfos>();
	return *queueInfos.at(ZQueueType::Graphics);
}

ZQueue VulkanContext::getComputeQueue() const
{
	add_cref<ZQueueInfos> queueInfos = device.getParamRef<ZQueueInfos>();
	return *queueInfos.at(ZQueueType::Compute);
}

uint32_t VulkanContext::getGraphicsQueueFamilyIndex () const
{
	return getGraphicsQueue().getParam<uint32_t>();
}

uint32_t VulkanContext::getComputeQueueFamilyIndex () const
{
	return getComputeQueue().getParam<uint32_t>();
}

const strings& VulkanContext::getAvailableInstanceExtensions() const
{
	return  instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>().data;
}

const strings& VulkanContext::getAvailablePhysicalDeviceExtensions() const
{
	return physicalDevice.getParamRef<strings>();
}

ZPipeline VulkanContext::createComputePipeline (ZPipelineLayout layout, ZShaderModule computeShaderModule, const char* entryName)
{
	VkPipelineShaderStageCreateInfo	sci{};
	sci.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sci.pNext	= nullptr;
	sci.flags	= VkPipelineShaderStageCreateFlags(0);
	sci.stage	= VK_SHADER_STAGE_COMPUTE_BIT;
	sci.module	= *computeShaderModule;
	sci.pName	= entryName;
	sci.pSpecializationInfo	= static_cast<const VkSpecializationInfo*>(nullptr);

	VkComputePipelineCreateInfo	ci{};
	ci.sType	= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.pNext	= nullptr;
	ci.flags	= VkPipelineCreateFlags(0);
	ci.stage	= sci;
	ci.layout	= *layout;
	ci.basePipelineHandle	= VK_NULL_HANDLE;
	ci.basePipelineIndex	= 0;

	ZDevice aDevice = layout.getParam<ZDevice>();

	ZPipeline	computePipeline (aDevice, callbacks, layout, VK_PIPELINE_BIND_POINT_COMPUTE);
	VKASSERT2(vkCreateComputePipelines(*aDevice, VkPipelineCache(VK_NULL_HANDLE), 1u, &ci, callbacks, computePipeline.setter()));

	return computePipeline;
}

ZPipeline VulkanContext::createGraphicsPipeline	(ZPipelineLayout layout, ZRenderPass renderPass, std::optional<VkExtent2D> extent,
												 ZShaderModule vertShaderModule, ZShaderModule fragShaderModule, bool enableDepthTest)
{
	return createGraphicsPipeline(layout, renderPass, extent, vertShaderModule, fragShaderModule, {}, {}, {},
								  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, enableDepthTest);
}

ZPipeline VulkanContext::createGraphicsPipeline (ZPipelineLayout layout, ZRenderPass renderPass, std::optional<VkExtent2D> extent,
												 ZShaderModule vertShaderModule, ZShaderModule fragShaderModule,
												 std::optional<ZShaderModule> tessCtrlModule, std::optional<ZShaderModule> tessEvalModule,
												 std::optional<ZShaderModule> geomModule,
												 VkPrimitiveTopology topology, bool enableDepthTest, uint32_t patchControlPoints)
{
	ZDevice						localDevice				(layout.getParam<ZDevice>());
	VkAllocationCallbacksPtr	localCallbacks			(localDevice.getParam<VkAllocationCallbacksPtr>());
	ZPipeline					graphicsPipeline		(localDevice, localCallbacks, layout, VK_PIPELINE_BIND_POINT_GRAPHICS);
	const uint32_t				renderAttachmentCount	= renderPass.getParam<uint32_t>();
	bool						enableTesselation		= false;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	{
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module	= *vertShaderModule;
		vertShaderStageInfo.pName	= "main";
		shaderStages.emplace_back(vertShaderStageInfo);
	}

	{
		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module	= *fragShaderModule;
		fragShaderStageInfo.pName	= "main";
		shaderStages.emplace_back(fragShaderStageInfo);
	}

	if (tessCtrlModule.has_value())
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage	= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		shaderStageInfo.module	= **tessCtrlModule;
		shaderStageInfo.pName	= "main";
		shaderStages.emplace_back(shaderStageInfo);
		enableTesselation = true;
	}

	if (tessEvalModule.has_value())
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage	= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		shaderStageInfo.module	= **tessEvalModule;
		shaderStageInfo.pName	= "main";
		shaderStages.emplace_back(shaderStageInfo);
		enableTesselation = true;
	}

	if (geomModule.has_value())
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage	= VK_SHADER_STAGE_GEOMETRY_BIT;
		shaderStageInfo.module	= **geomModule;
		shaderStageInfo.pName	= "main";
		shaderStages.emplace_back(shaderStageInfo);
	}

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo = m_vertexInput.createPipelineVertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology	= topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineTessellationStateCreateInfo tessellationState{};
	tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationState.pNext = nullptr;
	tessellationState.flags = 0;
	tessellationState.patchControlPoints = patchControlPoints;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = extent.has_value() ? (float) extent->width : 0;
	viewport.height = extent.has_value() ? (float) extent->height : 0;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = +1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	if (extent.has_value()) scissor.extent = *extent;

	const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pNext				= nullptr;
	dynamicState.flags				= 0;
	dynamicState.dynamicStateCount	= 2;
	dynamicState.pDynamicStates		= dynamicStates;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount	= 1;
	viewportState.pViewports	= extent.has_value() ? &viewport : nullptr;
	viewportState.scissorCount	= 1;
	viewportState.pScissors		= extent.has_value() ? &scissor : nullptr;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable			= VK_FALSE;
	rasterizer.rasterizerDiscardEnable	= VK_FALSE;
	rasterizer.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth				= 1.0f;
	//rasterizer.rasterizerDiscardEnable	= VK_TRUE;
	rasterizer.cullMode					= VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace				= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable			= VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentTemplate{};
	colorBlendAttachmentTemplate.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentTemplate.blendEnable	= VK_FALSE;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(renderAttachmentCount, colorBlendAttachmentTemplate);

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.logicOp			= VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount	= renderAttachmentCount;
	colorBlending.pAttachments		= colorBlendAttachments.data();
	colorBlending.blendConstants[0]	= 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

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

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount				= static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages				= shaderStages.data();
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pTessellationState		= enableTesselation ? &tessellationState : nullptr;
	pipelineInfo.pViewportState			= &viewportState;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= enableDepthTest ? &depthStencilState : nullptr;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= extent.has_value() ? nullptr : &dynamicState;
	pipelineInfo.layout					= *layout;
	pipelineInfo.renderPass				= *renderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= 0;

	VKASSERT2(vkCreateGraphicsPipelines(*localDevice, VkPipelineCache(0), 1, &pipelineInfo, localCallbacks, graphicsPipeline.setter()));

	return graphicsPipeline;
}

ZFence VulkanContext::createFence (bool signaled)
{
	return ::vtf::createFence(device, signaled);
}

ZSemaphore VulkanContext::createSemaphore ()
{
	return ::vtf::createSemaphore(device);
}

ZCommandPool VulkanContext::createGraphicsCommandPool ()
{
	return createCommandPool(device, getGraphicsQueue());
}

ZCommandPool VulkanContext::createComputeCommandPool ()
{
	return createCommandPool(device, getComputeQueue());
}

ZCommandBuffer VulkanContext::createCommandBuffer (ZCommandPool commandPool)
{
	return ::vtf::createCommandBuffer(commandPool);
}

ZBuffer	VulkanContext::createBuffer (ZImage image, ZBufferUsageFlags usage, uint32_t baseLevel, uint32_t levels, ZMemoryPropertyFlags properties) const
{
	const VkImageCreateInfo& ici = image.getParamRef<VkImageCreateInfo>();
	return ::vtf::createBuffer(device, image, usage, baseLevel, (((~0u) == levels) ? ici.mipLevels : levels), properties);
}

ZBuffer	VulkanContext::createBuffer (VkDeviceSize size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties) const
{
	return ::vtf::createBuffer(device, size, usage, properties);
}

ZImage	VulkanContext::createImage (VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
									uint32_t mipLevels, uint32_t layers, VkSampleCountFlagBits samples, VkMemoryPropertyFlags properties) const
{
	return ::vtf::createImage(device, format, width, height, usage, mipLevels, layers, samples, properties);
}

ZImageView	VulkanContext::createImageView (ZImage image, VkImageAspectFlags aspect, VkFormat chgfmt,
											uint32_t baseLevel, uint32_t levels,
											uint32_t baseLayer, uint32_t layers) const
{
	return ::vtf::createImageView(image, aspect, chgfmt, baseLevel, levels, baseLayer, layers);
}

ZSampler VulkanContext::createSampler (ZImageView view, bool filterLinearORnearest, bool normalized, bool mipMapEnable, bool anisotropyEnable) const
{
	uint32_t				mipLevels	= view.getParam<ZImage>().getParamRef<VkImageCreateInfo>().mipLevels;
	VkFilter				filter		= VK_FILTER_NEAREST;
	VkSamplerMipmapMode		mipMapMode	= VK_SAMPLER_MIPMAP_MODE_NEAREST;
	if (filterLinearORnearest)
	{
		VkFormatProperties				formatProperties;
		const VkFormat					format		= view.getParamRef<VkImageViewCreateInfo>().format;
		// test if image format supports linear blitting
		vkGetPhysicalDeviceFormatProperties(*physicalDevice, format, &formatProperties);
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
		{
			filter		= VK_FILTER_LINEAR;
			mipMapMode	= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		else
		{
			ASSERTMSG(0, "Linear filtering is not supported for that format");
		}
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.pNext					= nullptr;
	if (mipMapEnable)
	{
		samplerInfo.minLod				= 0.0f;
		samplerInfo.maxLod				= static_cast<float>(mipLevels);
		samplerInfo.mipLodBias			= 0.0f;
	}
	else
	{
		samplerInfo.minLod				= 0.0f;
		samplerInfo.maxLod				= normalized ? 0.225f : 0.0f;
		samplerInfo.mipLodBias			= 0.0f;
	}
	samplerInfo.addressModeU			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.borderColor				= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	if (anisotropyEnable)
	{
		VkPhysicalDeviceProperties	props{};
		vkGetPhysicalDeviceProperties(*physicalDevice, &props);
		samplerInfo.anisotropyEnable	= VK_TRUE;
		samplerInfo.maxAnisotropy		= props.limits.maxSamplerAnisotropy;
	}
	else
	{
		samplerInfo.anisotropyEnable	= VK_FALSE;
		samplerInfo.maxAnisotropy		= 0.0f;
	}
	samplerInfo.magFilter				= filter;
	samplerInfo.minFilter				= filter;
	samplerInfo.mipmapMode				= mipMapMode;
	samplerInfo.compareEnable			= VK_FALSE;
	samplerInfo.compareOp				= VK_COMPARE_OP_ALWAYS;
	samplerInfo.unnormalizedCoordinates	= normalized ? VK_FALSE : VK_TRUE;

	return ::vtf::createSampler(device, samplerInfo);
}

ZFramebuffer VulkanContext::createFramebuffer (ZRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<ZImageView>& attachments)
{
	return createFramebufferImpl(renderPass, width, height, attachments.data(), static_cast<uint32_t>(attachments.size()));
}

ZFramebuffer VulkanContext::createFramebufferImpl (ZRenderPass renderPass, uint32_t width, uint32_t height, const ZImageView* attachments, uint32_t attachmentCount)
{
	const uint32_t renderAttachmentCount = renderPass.getParam<uint32_t>();

	ASSERTION(attachmentCount >= renderAttachmentCount);
	VkPhysicalDeviceProperties	props{};
	vkGetPhysicalDeviceProperties(*physicalDevice, &props);
	ASSERTION(attachmentCount <= props.limits.maxFragmentOutputAttachments && attachmentCount <= props.limits.maxColorAttachments);

	VkFramebuffer	framebuffer = VK_NULL_HANDLE;

	std::vector<VkImageView>	views(attachmentCount);
	for (uint32_t i = 0; i < attachmentCount; ++i)
	{
		views[i] = *attachments[i];
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass		= *renderPass;
	framebufferInfo.attachmentCount	= attachmentCount;
	framebufferInfo.pAttachments	= views.data();
	framebufferInfo.width			= width;
	framebufferInfo.height			= height;
	framebufferInfo.layers			= 1;

	VKASSERT2(vkCreateFramebuffer(*device, &framebufferInfo, callbacks, &framebuffer));

	return ZFramebuffer::create(framebuffer, device, callbacks);
}

VkRenderPassBeginInfo VulkanContext::makeRenderPassBeginInfo(ZRenderPass rp, ZFramebuffer fb, const VkExtent2D& extent) const
{
	auto&					clearColors		= rp.getParamRef<std::vector<VkClearValue>>();
	const VkClearValue*		pClearValues	= clearColors.size() ? clearColors.data() : nullptr;
	VkRenderPassBeginInfo	renderPassInfo{};

	renderPassInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass			= *rp;
	renderPassInfo.framebuffer			= *fb;
	renderPassInfo.renderArea.offset	= {0, 0};
	renderPassInfo.renderArea.extent	= extent;
	renderPassInfo.clearValueCount		= static_cast<uint32_t>(clearColors.size());
	renderPassInfo.pClearValues			= pClearValues;
	return renderPassInfo;
}

VkFormat selectBestDepthStencilFormat (ZPhysicalDevice					device,
									   std::initializer_list<VkFormat>	formats,
									   VkImageTiling					imageTiling,
									   VkFormatFeatureFlags				featuresFlags)
{
	VkFormatProperties	props;
	for (auto i = formats.begin(); i != formats.end(); ++i)
	{
		vkGetPhysicalDeviceFormatProperties(*device, *i, &props);

		VkFormatFeatureFlags features = 0;
		switch (imageTiling)
		{
			case VK_IMAGE_TILING_LINEAR:
				features = props.linearTilingFeatures;
				break;
			case VK_IMAGE_TILING_OPTIMAL:
				features = props.optimalTilingFeatures;
				break;
			default: ASSERTION(0);
		}

		if ((features & featuresFlags) == featuresFlags)
			return *i;
	}
	ASSERTION(0);
	return VK_FORMAT_UNDEFINED;
}

ZRenderPass VulkanContext::createRenderPass (std::vector<VkFormat> colorFormats, std::optional<VkClearValue> clearColor,
											 VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
											 bool enableDepthTest, float maxDepth)
{
	const uint32_t colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());

	ASSERTION(colorFormats.size());

	const VkFormat dsFormat = enableDepthTest
								? selectBestDepthStencilFormat(
									physicalDevice,
									{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
									VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT },
									VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
								: VK_FORMAT_UNDEFINED;
	VkAttachmentDescription depthAttachmentTemplate{};
	depthAttachmentTemplate.flags			= 0;
	depthAttachmentTemplate.format			= dsFormat;
	depthAttachmentTemplate.samples			= VK_SAMPLE_COUNT_1_BIT;
	depthAttachmentTemplate.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentTemplate.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachmentTemplate.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentTemplate.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentTemplate.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentTemplate.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference	depthAttachmentRef{};
	depthAttachmentRef.attachment	= colorAttachmentCount;
	depthAttachmentRef.layout		= depthAttachmentTemplate.finalLayout;

	VkAttachmentDescription colorAttachmentTemplate{};
	colorAttachmentTemplate.flags			= 0;
	colorAttachmentTemplate.format			= VK_FORMAT_UNDEFINED;
	colorAttachmentTemplate.samples			= VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentTemplate.loadOp			= clearColor.has_value()
												? VK_ATTACHMENT_LOAD_OP_CLEAR
												: (VK_IMAGE_LAYOUT_UNDEFINED != initialColorLayout)
													? VK_ATTACHMENT_LOAD_OP_LOAD
													: VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentTemplate.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentTemplate.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentTemplate.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentTemplate.initialLayout	= initialColorLayout;
	colorAttachmentTemplate.finalLayout		= finalColorLayout;

	std::vector<VkAttachmentDescription> attachments(colorAttachmentCount, colorAttachmentTemplate);

	std::vector<VkAttachmentReference> colorAttachmentRefs(colorAttachmentCount);
	for (auto i = colorFormats.begin(); i != colorFormats.end(); ++i)
	{
		const uint32_t j = static_cast<uint32_t>(std::distance(colorFormats.begin(), i));
		attachments[j].format				= *i;
		colorAttachmentRefs[j].attachment	= j;
		colorAttachmentRefs[j].layout		= finalColorLayout;
	}

	if (enableDepthTest)
	{
		attachments.emplace_back(depthAttachmentTemplate);
	}

	VkSubpassDescription subpass{};
	subpass.flags					= 0;
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pInputAttachments		= nullptr;
	subpass.colorAttachmentCount	= colorAttachmentCount;
	subpass.pColorAttachments		= colorAttachmentRefs.data();
	subpass.pDepthStencilAttachment	= enableDepthTest ? &depthAttachmentRef : nullptr;
	subpass.inputAttachmentCount	= 0;
	subpass.pResolveAttachments		= nullptr;
	subpass.preserveAttachmentCount	= 0;
	subpass.pPreserveAttachments	= nullptr;


	VkSubpassDependency	dependencies[2];
	// color dependency
	{
		dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass		= 0;
		dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask	= dependencies[0].srcStageMask;
		dependencies[0].srcAccessMask	= 0;
		dependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	}

	// depth dependency
	{
		dependencies[1].srcSubpass		= VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass		= 0;
		dependencies[1].srcStageMask	= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask	= dependencies[1].srcStageMask;
		dependencies[1].srcAccessMask	= 0;
		dependencies[1].dstAccessMask	= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;
	}

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount	= uint32_t(attachments.size());
	renderPassInfo.pAttachments		= attachments.data();
	renderPassInfo.subpassCount		= 1;
	renderPassInfo.pSubpasses		= &subpass;
	renderPassInfo.dependencyCount	= enableDepthTest ? 2 : 1;
	renderPassInfo.pDependencies	= dependencies;

	ZRenderPass	renderPass(device, callbacks, colorAttachmentCount, {}, false, VK_FORMAT_UNDEFINED);
	VKASSERT2(vkCreateRenderPass(*device, &renderPassInfo, callbacks, renderPass.setter()));

	auto& clearValues = renderPass.getParamRef<std::vector<VkClearValue>>();
	if (clearColor.has_value())
	{		
		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
			clearValues.emplace_back(*clearColor);
	}
	if (enableDepthTest)
	{
		renderPass.getParamRef<bool>()		= true;
		renderPass.getParamRef<VkFormat>()	= depthAttachmentTemplate.format;
		VkClearValue clearDepth{};
		clearDepth.depthStencil.depth = maxDepth;
		clearValues.emplace_back(clearDepth);
	}

	return renderPass;
}

} // namespace vtf
