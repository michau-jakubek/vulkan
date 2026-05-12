#include "simpleDRLRTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfCommandLine.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	uint32_t attachmentCount = 4u;
	uint32_t frameSize = 4u;
	bool KHR = false;
	bool synchronization2 = true;
	bool buildAlways = false;
	Params(add_cref<std::string> assets_)
		: assets(assets_) {}
	OptionParser<Params> getParser();
};
constexpr Option optionBuildAlways{ "--build-always", 0 };
OptionParser<Params> Params::getParser()
{
	OptionFlags	flagsDef(OptionFlag::PrintValueAsDefault);
	auto p = OptionParser<Params>(*this);
	p.addOption(&Params::buildAlways, optionBuildAlways,
		"Rebuild the shaders each time you run application", { false }, flagsDef)
		->setParamName("buildAlways");
	return p;
}

TriLogicInt prepareTests(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine);
TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);
	OptionParser<Params> parser = params.getParser();
	parser.parse(cmdLine);
	OptionParserState state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout, 60);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messages.str() << std::endl;
		if (state.hasErrors) return {};
	}

	uint32_t maxColorAttachments = 0u;
	uint32_t maxPerStageDescriptorInputAttachments = 0u;

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		add_cref<VkPhysicalDeviceProperties> props = deviceGetPhysicalProperties(caps.physicalDevice);
		const uint32_t major = VK_VERSION_MAJOR(props.apiVersion);
		const uint32_t minor = VK_VERSION_MINOR(props.apiVersion);
		params.KHR = major == 1u && minor < 4u;

		if (params.KHR)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingLocalReadFeatures::dynamicRenderingLocalRead)
				.checkSupported("dynamicRenderingLocalRead");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
				.checkSupported("dynamicRendering");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState)
				.checkSupported("extendedDynamicState");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicState2FeaturesEXT::extendedDynamicState2)
				.checkSupported("extendedDynamicState2");
			//caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicState3FeaturesEXT::)
			//	.checkSupported("extendedDynamicState");

			if (params.synchronization2)
			{
				caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
					.checkSupported("synchronization2");
				caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
			}
		}
		else
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan14Features::dynamicRenderingLocalRead)
				.checkSupported("dynamicRenderingLocalRead");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::dynamicRendering)
				.checkSupported("dynamicRendering");
			/*
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::maintenance4)
				.checkSupported("maintenance4");
			caps.addExtension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME).checkSupported();
			*/

			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::descriptorBindingSampledImageUpdateAfterBind)
				.checkSupported("descriptorBindingSampledImageUpdateAfterBind");
			caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::descriptorIndexing)
				.checkSupported("descriptorIndexing");

			if (params.synchronization2)
			{
				caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan13Features::synchronization2)
					.checkSupported("synchronization2");
				caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
			}
		}
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME).checkSupported();

		caps.addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387]
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[1]
		// Missing extension required by the device extension VK_KHR_dynamic_rendering : VK_KHR_depth_stencil_resolve.
		caps.addExtension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387]
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[2]
		// Missing extension required by the device extension VK_KHR_depth_stencil_resolve : VK_KHR_create_renderpass2.
		caps.addExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);

		// maxColorAttachments
		// maxFragmentOutputAttachments
		// maxDescriptorSetInputAttachments
		add_cref<VkPhysicalDeviceLimits> limits = deviceGetPhysicalLimits(caps.physicalDevice);
		maxColorAttachments = limits.maxColorAttachments;
		maxPerStageDescriptorInputAttachments = limits.maxPerStageDescriptorInputAttachments;
		const uint32_t attachmentsLimit = std::min(maxColorAttachments, maxPerStageDescriptorInputAttachments);
		params.attachmentCount = std::clamp(params.attachmentCount, 2u, attachmentsLimit);
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, gf.apiVer);
	return runTests(cs, params);
}

std::array<ZShaderModule, 3> genShaders(ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "write.frag");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "read.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer,
							flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);
	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
	};
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
	std::cout << "KHR enabled: " << params.KHR << std::endl;

	add_cref<ZDeviceInterface>	di			= canvas.device.getInterface();

	const auto					[vs, write_fs, read_fs]	= genShaders(canvas.device, params);

	ZCommandPool				cmdPool		= createCommandPool(canvas.device, canvas.graphicsQueue);
	LayoutManager				lm			(canvas.device, cmdPool);

	std::vector<ZImage>			inputImages(params.attachmentCount);
	std::vector<ZImageView>		inputAttachments(params.attachmentCount);
	std::vector<ZBuffer>		inputBuffers(params.attachmentCount);
	std::vector<ZBuffer>		outputBuffers(params.attachmentCount);

	std::vector<gpp::Attachment>	inputRenderingAttachments(params.attachmentCount);
	std::vector<VkClearValue>		clearColors(params.attachmentCount);
	std::vector<gpp::BlendAttachmentState> readPipelineBlendStates(params.attachmentCount);

	using S = ZBarrierConstants::Stage;
	using A = ZBarrierConstants::Access;

	ZMemoryBarrier  localReadBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	ZMemoryBarrier2 localReadBarrier2(A::COLOR_ATTACHMENT_WRITE_BIT, S::COLOR_ATTACHMENT_OUTPUT_BIT,
										A::INPUT_ATTACHMENT_READ_BIT, S::FRAGMENT_SHADER_BIT);

	std::vector<ZImageMemoryBarrier>	transitGeneralInputBarriers(params.attachmentCount);
	std::vector<ZImageMemoryBarrier2>	transitGeneralInputBarriers2(params.attachmentCount);

	const VkImageLayout inputAttachmentsLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ;

	struct PushConstant { uint32_t width; } const pc{ params.frameSize };
	ZPushConstants				pushConstants(ZPushRange<PushConstant>(shaderGetStage(write_fs)));

	for (uint32_t i = 0u; i < params.attachmentCount; ++i)
	{
		clearColors[i] = makeClearColor(UVec4(i + 1u, i + 2u, i + 3u, i + 4u));

		auto blendAttachmentState = gpp::defaultBlendAttachmentState;
		blendAttachmentState.colorWriteMask = 0u;

		inputImages[i] = canvas.createColorImage2D(VK_FORMAT_R32_UINT, params.frameSize, params.frameSize);
		inputAttachments[i] = createImageView(inputImages[i]);

		inputBuffers[i] = createBuffer(inputImages[i]);
		outputBuffers[i] = createBuffer(inputImages[i]);

		lm.addBinding(i, inputAttachments[i], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
												inputAttachmentsLayout, shaderGetStage(write_fs));
		lm.addBinding(1 * params.attachmentCount + i,
			inputBuffers[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderGetStage(write_fs));
		lm.addBinding(2 * params.attachmentCount + i,
			outputBuffers[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderGetStage(write_fs));

		inputRenderingAttachments[i] = gpp::Attachment(inputAttachments[i], gpp::AttachmentDesc::Color);
		readPipelineBlendStates[i] = gpp::BlendAttachmentState(std::make_pair(i, blendAttachmentState));

		transitGeneralInputBarriers[i] = ZImageMemoryBarrier(inputImages[i],
											VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, inputAttachmentsLayout);
		transitGeneralInputBarriers2[i] = ZImageMemoryBarrier2(inputImages[i],
											A::NONE, S::TOP_OF_PIPE_BIT,
											A::COLOR_ATTACHMENT_WRITE_BIT, S::FRAGMENT_SHADER_BIT, inputAttachmentsLayout);
	}

	const std::vector<uint32_t> readPipelineInputAttachmentIndices{ 3, 0, 2, 1 };

	ZPipelineLayout				writeLayout = lm.createPipelineLayout(pushConstants);
	ZPipeline					writePipeline =
		createGraphicsPipeline(writeLayout, vs, write_fs,
			inputRenderingAttachments,
			gpp::RenderingInpuAttachmentIndices(&readPipelineInputAttachmentIndices),
			gpp::SubpassIndex(0),
			makeExtent2D(params.frameSize, params.frameSize),
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	ZDescriptorSetLayout		dsLayout = lm.createDescriptorSetLayout(true,
		VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT));
	ZPipelineLayout				readLayout = lm.createPipelineLayout({ dsLayout }, pushConstants);
	ZPipeline					readPipeline =
		createGraphicsPipeline(readLayout, vs, read_fs,
			inputRenderingAttachments,
			gpp::RenderingInpuAttachmentIndices(&readPipelineInputAttachmentIndices),
			gpp::SubpassIndex(1), readPipelineBlendStates,
			makeExtent2D(params.frameSize, params.frameSize),
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	{
		OneShotCommandBuffer cmd(cmdPool);
		commandBufferPipelineBarriers(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, transitGeneralInputBarriers);
		commandBufferBeginRendering(cmd, params.frameSize, params.frameSize, inputRenderingAttachments, clearColors);
		commandBufferSetRenderingInputAttachmentIndices(cmd, readPipelineInputAttachmentIndices, {}, {}, true);
		commandBufferPushConstants(cmd, writeLayout, pc);
		commandBufferBindPipeline(cmd, writePipeline);
		di.vkCmdDraw(**cmd, 4u, 1u, 0u, 0u);
		commandBufferPipelineBarriers(cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, localReadBarrier);
		commandBufferPushConstants(cmd, readLayout, pc);
		commandBufferBindPipeline(cmd, readPipeline);
		di.vkCmdDraw(**cmd, 4u, 1u, 0u, 0u);
		commandBufferEndRendering(cmd);
	}

	std::vector<uint32_t> v;
	lm.readBinding(4, v);
	for (uint32_t i = 0; i < v.size(); ++i)
		std::cout << i << ":" << v[i] << ' ';
	std::cout << std::endl;
	lm.readBinding(8, v);
	for (uint32_t i = 0; i < v.size(); ++i)
		std::cout << i << ":" << v[i] << ' ';
	std::cout << std::endl;

	return 1;
}

} // unnamed namespace

template<> struct TestRecorder<SIMPLE_DRLR>
{
	static bool record(TestRecord&);
};
bool TestRecorder<SIMPLE_DRLR>::record(TestRecord& record)
{
	record.name = "simple_drlr";
	record.call = &prepareTests;
	return true;
}
