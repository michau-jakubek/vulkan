#include "intSynchronization2.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfContext.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZBarriers.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfOptionParser.hpp"

#include <bitset>
#include <numeric>

namespace
{
using namespace vtf;
using Programs = ProgramCollection;

struct Params
{
	add_cref<std::string>	assets;
	bool					dummy;

	Params (add_cref<std::string>	assets_)
		: assets	(assets_)
		, dummy		(false) {}
	OptionParser<Params> getParser ();
};
constexpr Option optionDummy { "--dummy", 0 };
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags	(OptionFlag::PrintDefault);
	OptionParser<Params>	parser	(*this);
	parser.addOption(&Params::dummy, optionDummy,
					 "Reserved for future use", { dummy }, flags);
	return parser;
}

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams);
TriLogicInt runSynchronization2Tests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests(const TestRecord& record, const strings& cmdLineParams)
{
	Params					params	(record.assets);
	OptionParser<Params>	parser	= params.getParser();
	OptionParserState		state	{};

	parser.parse(cmdLineParams);
	state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messagesText() << std::endl;
		if (state.hasErrors) return {};
	}

	VkPhysicalDeviceSynchronization2Features	synchFeatures = makeVkStruct();

	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		VkPhysicalDeviceFeatures2 availableFeatures =
				deviceGetPhysicalFeatures2(physicalDevice, &synchFeatures);
		synchFeatures.synchronization2 |= (containsString(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
														  extensions) ? VK_TRUE : VK_FALSE);
		availableFeatures.features = {};
		return availableFeatures;
	};

	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();
	VulkanContext				ctx(record.name, gf.layers, strings(), strings(), onEnablingFeatures, gf.apiVer);

	if (VK_FALSE == synchFeatures.synchronization2)
	{
		std::cout << "ERROR: " VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME " is not supported by device" << std::endl;
		return (-1);
	}

	return runSynchronization2Tests(ctx, params);
}

UNUSED add_cptr<char> pipelineStage2ToString (VkFlags64 stage)
{
	std::bitset<sizeof(stage) * 8> bits(stage);
	ASSERTMSG(bits.count() <= 1, "???");

	struct PipeStage2Entry
	{
		const VkFlags64 flags;
		add_cptr<char> name;
		PipeStage2Entry(VkFlags64 flags_, add_cptr<char> name_)
			: flags(flags_), name(name_) {}
	};
#define MKPIPESTAGE2ENTRY(x) PipeStage2Entry(x, #x)

	const PipeStage2Entry entries[]
	{
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_NONE),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_NONE_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TRANSFER_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_HOST_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_HOST_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COPY_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COPY_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_RESOLVE_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_BLIT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_BLIT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_CLEAR_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_SHADING_RATE_IMAGE_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT),
//		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_SUBPASS_SHADING_BIT_HUAWEI),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_CLUSTER_CULLING_SHADER_BIT_HUAWEI),
		MKPIPESTAGE2ENTRY(VK_PIPELINE_STAGE_2_OPTICAL_FLOW_BIT_NV),
	};

	for (add_cref<PipeStage2Entry> entry : entries)
	{
		if (entry.flags == stage) return entry.name;
	}

	return nullptr;
}

template<class Draw, class... Args>
ZCommandBuffer beginEndCommand (add_ref<VulkanContext> ctx, ZCommandPool cmdPool, Draw&& draw, Args&&... args)
{
	UNREF(ctx);
	ZCommandBuffer cmd = allocateCommandBuffer(cmdPool);
	commandBufferBegin(cmd);
		draw(cmd, std::forward<Args>(args)...);
	commandBufferEnd(cmd);
	commandBufferSubmitAndWait(cmd);
	return cmd;
}
template<class MakeCmd, class Draw, class... Args>
void execute (add_ref<VulkanContext> ctx, ZCommandPool cmdPool, MakeCmd&& makeCmd, Draw& draw, Args&&... args)
{
	ZCommandBuffer cmd = makeCmd(ctx, cmdPool, std::forward<Draw>(draw), std::forward<Args>(args)...);
}

TriLogicInt runSynchronization2Tests (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
	LayoutManager		lm				(ctx.device);
	VertexInput			vertexInput		(ctx.device);
	Programs			programs		(ctx.device, params.assets);

	const uint32_t		width			= 256;
	const uint32_t		height			= 256;
	const VkExtent2D	extent			{ width, height };
	const VkFormat		format			= VK_FORMAT_R32_UINT;
	ZImage				colorImage		= ctx.createColorImage2D(format, width, height);
	ZImageView			colorView		= createImageView(colorImage);
	ZImage				storageImage	= createImage(ctx.device, format, VK_IMAGE_TYPE_2D, width, height,
											ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT));
	ZImageView			storageView		= createImageView(storageImage);

	vertexInput.binding(0).addAttributes(std::vector<Vec4>{ { -1, +3.5 }, { -1, -1 }, { +3.5, -1 } });

	const uint32_t		buffer0binding = lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (width * height));
	const uint32_t		buffer1binding = lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (width * height));
	const uint32_t		buffer2binding = lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (width * height));
	const uint32_t		storageBinding = lm.addBinding(storageView, ZSampler());	UNREF(storageBinding);

	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	programs.buildAndVerify(true);
	ZShaderModule		vertShader		= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule		fragShader		= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);
	ZShaderModule		compShader		= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);

	ZRenderPass			renderPass		= createColorRenderPass(ctx.device, { format });
	ZFramebuffer		framebuffer		= createFramebuffer(renderPass, extent, { colorView });	
	ZPipelineLayout		pipelineLayout	= lm.createPipelineLayout(lm.createDescriptorSetLayout());
	ZPipeline			graphPipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
											gpp::DepthWriteEnable(true), extent, vertexInput, vertShader, fragShader);
	ZPipeline			compPipeline	= createComputePipeline(pipelineLayout, compShader, UVec3(1));

	ZBuffer				buffer0			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(buffer0binding)).buffer;
	ZBuffer				buffer1			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(buffer1binding)).buffer;
	ZBuffer				buffer2			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(buffer2binding)).buffer;
	ZBuffer				indirectBuffer	= createBuffer<VkDrawIndexedIndirectCommand>(
												ctx.device, ZBufferUsageFlags(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));
	ZBuffer				indexBuffer		= createIndexBuffer(ctx.device, 3, VK_INDEX_TYPE_UINT32);
	
	/*
	bufferWrite(, VkDrawIndirectCommand{
		vertexInput.getAttributeCount(0),	// vertexCount
		1u,									// instanceCount
		0u,									// firstVertex
		0u,									// firstInstance					
	});
	*/
	bufferWrite(indexBuffer, { 0u, 1u, 2u });
	bufferWrite(indirectBuffer, VkDrawIndexedIndirectCommand{
		bufferGetElementCount<uint32_t>(indexBuffer),	// indexCount
		1u,												// instanceCount
		0u,												// firstIndex
		0u,												// vertexOffset
		0u,												// firstInstance
	});

	std::cout << "Buffer0:        " << buffer0 << std::endl;
	std::cout << "Buffer1:        " << buffer1 << std::endl;
	std::cout << "Buffer2:        " << buffer2 << std::endl;
	std::cout << "indirectBuffer: " << indirectBuffer << std::endl;
	std::cout << "indexBuffer:    " << indexBuffer << std::endl;
	std::cout << "Vertex:         " << vertShader << std::endl;
	std::cout << "Fragment:       " << fragShader << std::endl;
	std::cout << "Compute:        " << compShader << std::endl;
	std::cout << "Color:          " << colorImage << std::endl;
	std::cout << "Storage:        " << storageImage << std::endl;

	// 1. copy buffer0 to buffer1
	// 2. compute copy buffer1 to image0
	// 3. renderpass sample image0 to framebuffer
	// 4. host copy framebuffer to buffer2

	typedef ZBarrierConstants::Access	A;
	typedef ZBarrierConstants::Stage	S;

	ZImageMemoryBarrier2 b2(storageImage, A::NONE, S::TOP_OF_PIPE_BIT,
										A::SHADER_WRITE_BIT, S::COMPUTE_SHADER_BIT,
										VK_IMAGE_LAYOUT_GENERAL);

	ZImageMemoryBarrier2 b3(storageImage, A::SHADER_WRITE_BIT, S::COMPUTE_SHADER_BIT,
										A::SHADER_READ_BIT, S::FRAGMENT_SHADER_BIT,
										VK_IMAGE_LAYOUT_GENERAL);

	auto draw = [&](ZCommandBuffer cmd) -> void
	{
		// assume that command buffer is in a recording state

		commandBufferBindPipeline(cmd, compPipeline);
		commandBufferBindPipeline(cmd, graphPipeline);
		commandBufferBindVertexBuffers(cmd, vertexInput);
		commandBufferBindIndexBuffer(cmd, indexBuffer);

		bufferCopyToBuffer2(cmd, buffer0, buffer1,
							A::NONE, A::SHADER_READ_BIT,
							S::TOP_OF_PIPE_BIT, S::COMPUTE_SHADER_BIT);

		commandBufferPipelineBarriers2(cmd, b2);
		commandBufferDispatch(cmd, UVec3(width, height, 1u));
		commandBufferPipelineBarriers2(cmd, b3);

		auto rpbi = commandBufferBeginRenderPass(cmd, framebuffer, 0);
			commandBufferDrawIndexedIndirect(cmd, indirectBuffer);
		commandBufferEndRenderPass(rpbi);

		imageCopyToBuffer2(cmd, colorImage, buffer2,
								A::COLOR_ATTACHMENT_READ_BIT, A::NONE,
								A::NONE, A::NONE,
								S::COLOR_ATTACHMENT_OUTPUT_BIT, S::BOTTOM_OF_PIPE_BIT);

		// assume that command buffer recording will be finished and submitted
	};

	std::vector<uint32_t> buffer0data(lm.getBindingElementCount(buffer0binding));
	std::iota(buffer0data.begin(), buffer0data.end(), 1u);
	lm.writeBinding(buffer0binding, buffer0data);

	std::vector<uint32_t> buffer1data(lm.getBindingElementCount(buffer1binding));
	std::iota(buffer1data.begin(), buffer1data.end(), 2u);
	lm.writeBinding(buffer1binding, buffer1data);

	ZCommandPool graphicsCommandPool = ctx.createGraphicsCommandPool();

	execute(ctx, graphicsCommandPool, beginEndCommand<std::decay_t<decltype(draw)>>, draw);


	std::vector<uint32_t> buffer2data(lm.getBindingElementCount(buffer2binding));
	lm.readBinding(buffer1binding, buffer1data);
	lm.readBinding(buffer2binding, buffer2data);

	uint32_t mismatchIndex = INVALID_UINT32;
	for (uint32_t i = 0; i < data_count(buffer0data); ++i)
	{
		if (buffer2data.at(i) != buffer0data.at(i) ||
			buffer1data.at(i) != buffer0data.at(i))
		{
			mismatchIndex = i;
			break;
		}
	}

	if (INVALID_UINT32 == mismatchIndex)
	{
		return 0;
	}

	std::cout << "First mismatch at index: " << mismatchIndex
			  << ", expected: " << UVec2(buffer0data.at(mismatchIndex), buffer0data.at(mismatchIndex))
			  << " got: " << UVec2(buffer1data.at(mismatchIndex), buffer2data.at(mismatchIndex)) << std::endl;

	return 1;
}

} // unnamed namespace

template<> struct TestRecorder<INT_SYNCHRONIZATION2>
{
	static bool record(TestRecord&);
};
bool TestRecorder<INT_SYNCHRONIZATION2>::record(TestRecord& record)
{
	record.name = "int_synchronization2";
	record.call = &prepareTests;
	return true;
}

