#include "intSynchronization2.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfContext.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZBarriers.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfCommandLine.hpp"

#include <bitset>
#include <numeric>

namespace
{
using namespace vtf;
using Programs = ProgramCollection;

struct Params
{
	add_cref<std::string>	assets;
	bool					buildAlways;

	Params (add_cref<std::string>	assets_)
		: assets		(assets_)
		, buildAlways	(false) {}
	OptionParser<Params> getParser ();
};
constexpr Option optionBuildAlways { "--build-always", 0 };
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags	(OptionFlag::PrintDefault);
	OptionParser<Params>	parser	(*this);
	parser.addOption(&Params::buildAlways, optionBuildAlways,
					 "Force to build the shaders always", { buildAlways }, flags);
	return parser;
}

TriLogicInt prepareTests (const TestRecord& record, add_ref<CommandLine> cmdLine);
TriLogicInt runSynchronization2Tests (add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTests (const TestRecord& record, add_ref<CommandLine> cmdLine)
{
	Params					params	(record.assets);
	OptionParser<Params>	parser	= params.getParser();
	OptionParserState		state	{};

	parser.parse(cmdLine);
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

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
			.checkSupported("synchronization2");
		caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
	};

	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();
	VulkanContext				ctx(record.name, gf.layers, strings(), strings(), onEnablingFeatures, gf.apiVer);

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

template<class Recorder, class Drawer>
void beginEndCommand (add_ref<VulkanContext> ctx, ZCommandPool cmdPool, Recorder&& record, Drawer&& draw, ZFramebuffer fb, ZRenderPass rp)
{
	UNREF(ctx);
	UNREF(draw);
	UNREF(fb);
	UNREF(rp);

	ZCommandBuffer cmd = allocateCommandBuffer(cmdPool);
	commandBufferBegin(cmd);
		record(cmd, ZCommandBuffer());
	commandBufferEnd(cmd);
	commandBufferSubmitAndWait(cmd);
}
template<class Recorder, class Drawer>
void beginEndCommand2 (add_ref<VulkanContext> ctx, ZCommandPool cmdPool, Recorder&& record, Drawer&& draw, ZFramebuffer fb, ZRenderPass rp)
{
	UNREF(ctx);
	ZCommandBuffer primary = allocateCommandBuffer(cmdPool, true);
	ZCommandBuffer secondary = allocateCommandBuffer(cmdPool, false);

	commandBufferBegin(secondary, fb, rp);
		draw(secondary);
	commandBufferEnd(secondary);

	commandBufferBegin(primary);
		record(primary, secondary);
	commandBufferEnd(primary);
	commandBufferSubmitAndWait(primary);
}
template<class CmdMaker, class Recorder, class Drawer>
void execute (add_ref<VulkanContext> ctx, ZCommandPool cmdPool, CmdMaker&& makeCmd, Recorder&& record, Drawer&& draw, ZFramebuffer fb, ZRenderPass rp = {})
{
	makeCmd(ctx, cmdPool, std::move(record), std::move(draw), fb, rp);
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
	ZImage				colorImage		= createImage(ctx.device, format, VK_IMAGE_TYPE_2D, width, height,
													  ZImageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_STORAGE_BIT));
	ZImageView			colorView		= createImageView(colorImage);
	ZImage				storageImage	= createImage(ctx.device, format, VK_IMAGE_TYPE_2D, width, height,
														ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT));
	ZImageView			storageView	= createImageView(storageImage);

	vertexInput.binding(0).addAttributes(std::vector<Vec4>{ { -1, +3.5 }, { -1, -1 }, { +3.5, -1 } });

	ZBufferUsageFlags	busage	= ZBufferUsageStorageFlags;
	ZBuffer				buffer0 = createBuffer<uint32_t>(ctx.device, (width * height), busage);
	ZBuffer				buffer1 = createBuffer<uint32_t>(ctx.device, (width * height), busage);
	ZBuffer				buffer2 = createBuffer<uint32_t>(ctx.device, (width * height), busage);
	ZBuffer				buffer3 = createBuffer<uint32_t>(ctx.device, (width * height), busage);
	const uint32_t		buffer0binding	= lm.addBinding(buffer0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const uint32_t		buffer1binding	= lm.addBinding(buffer1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const uint32_t		buffer2binding	= lm.addBinding(buffer2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const uint32_t		buffer3binding	= lm.addBinding(buffer3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const uint32_t		storageBinding	= lm.addBinding(storageView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	const uint32_t		colorBinding	= lm.addBinding(colorView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "forward.comp");
	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "backward.comp");
	programs.buildAndVerify(true);
	ZShaderModule		vertShader		= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule		fragShader		= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);
	ZShaderModule		forwardShader	= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 0);
	ZShaderModule		backwardShader	= programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT, 1);

	const std::vector<RPA>		colors	{ RPA(AttachmentDesc::Color, format, {}) };
	const ZAttachmentPool		fbLayout(colors);
	const ZSubpassDescription2	subpass	({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZRenderPass			renderPass		= createRenderPass(ctx.device, fbLayout, subpass);
	ZFramebuffer		framebuffer		= createFramebuffer(renderPass, extent, { colorView });	
	ZPipelineLayout		pipelineLayout	= lm.createPipelineLayout({ lm.createDescriptorSetLayout() });
	ZPipeline			graphPipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
											extent, vertexInput, vertShader, fragShader);
	ZPipeline			forwardPipeline = createComputePipeline(pipelineLayout, forwardShader, {}, UVec3(1));
	ZPipeline			backwardPipeline = createComputePipeline(pipelineLayout, backwardShader, {}, UVec3(1));

	ZBuffer				indirectBuffer	= createBuffer<VkDrawIndexedIndirectCommand>(
												ctx.device, 1u, ZBufferUsageFlags(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));
	ZBuffer				indexBuffer		= createIndexBuffer(ctx.device, 3, VK_INDEX_TYPE_UINT32);
	
	/*
	bufferWrite(, VkDrawIndirectCommand{
		vertexInput.getAttributeCount(0),	// vertexCount
		1u,									// instanceCount
		0u,									// firstVertex
		0u,									// firstInstance					
	});
	*/
	const uint32_t indexBufferContent[3]{ 0u, 1u, 2u };
	bufferWrite(indexBuffer, indexBufferContent);
	bufferWrite(indirectBuffer, VkDrawIndexedIndirectCommand{
		bufferGetElementCount<uint32_t>(indexBuffer),	// indexCount
		1u,												// instanceCount
		0u,												// firstIndex
		0u,												// vertexOffset
		0u,												// firstInstance
	});

	std::cout << "Binding: " << buffer0binding << " Buffer0: " << buffer0 << std::endl;
	std::cout << "Binding: " << buffer1binding << " Buffer1: " << buffer1 << std::endl;
	std::cout << "Binding: " << buffer2binding << " Buffer2: " << buffer2 << std::endl;
	std::cout << "Binding: " << buffer3binding << " Buffer3: " << buffer3 << std::endl;
	std::cout << "Binding: " << storageBinding << " Storage: " << storageImage << std::endl;
	std::cout << "Binding: " << colorBinding   << " Color:   " << colorImage << std::endl;
	std::cout << "indirectBuffer: " << indirectBuffer << std::endl;
	std::cout << "indexBuffer:    " << indexBuffer << std::endl;
	std::cout << "Vertex:         " << vertShader << std::endl;
	std::cout << "Fragment:       " << fragShader << std::endl;
	std::cout << "Forward:        " << forwardShader << std::endl;
	std::cout << "Backward:       " << backwardShader << std::endl;

	typedef ZBarrierConstants::Access	A;
	typedef ZBarrierConstants::Stage	S;

	ZImageMemoryBarrier2 b1(storageImage, A::NONE, S::TOP_OF_PIPE_BIT,
										A::SHADER_WRITE_BIT, S::COMPUTE_SHADER_BIT,
										VK_IMAGE_LAYOUT_GENERAL);
	ZImageMemoryBarrier2 b2(storageImage, A::SHADER_WRITE_BIT, S::COMPUTE_SHADER_BIT,
										A::SHADER_READ_BIT, S::FRAGMENT_SHADER_BIT,
										VK_IMAGE_LAYOUT_GENERAL);
	ZImageMemoryBarrier2 b3(colorImage, A::COLOR_ATTACHMENT_WRITE_BIT, S::COLOR_ATTACHMENT_OUTPUT_BIT,
										A::SHADER_STORAGE_READ_BIT, S::COMPUTE_SHADER_BIT,
										VK_IMAGE_LAYOUT_GENERAL);

	auto draw = [&](ZCommandBuffer cmd) -> void
	{
		commandBufferBindPipeline(cmd, graphPipeline);
		commandBufferBindVertexBuffers(cmd, vertexInput);
		commandBufferBindIndexBuffer(cmd, indexBuffer);
		commandBufferDrawIndexedIndirect(cmd, indirectBuffer);
	};

	auto record = [&](ZCommandBuffer primary, ZCommandBuffer secondary) -> void
	{
		// assume that command buffer is in a recording state

		std::cout << "Primary command:   " << primary << std::endl;
		std::cout << "Secondary command: " << secondary << std::endl;

		bufferCopyToBuffer2(primary, buffer0, buffer1,
							A::NONE, A::SHADER_READ_BIT,
							S::TOP_OF_PIPE_BIT, S::COMPUTE_SHADER_BIT);

		commandBufferBindPipeline(primary, forwardPipeline);
		commandBufferPipelineBarriers2(primary, b1);
		commandBufferDispatch(primary, UVec3(width, height, 1u));
		commandBufferPipelineBarriers2(primary, b2);

		const VkSubpassContents contents = secondary.has_handle()
				? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE;
		auto rpbi = commandBufferBeginRenderPass(primary, framebuffer, contents);
			if (secondary.has_handle())
			{
				commandBufferExecuteCommands(primary, { secondary });
			}
			else
			{
				draw(primary);
			}
		commandBufferEndRenderPass(rpbi);

		commandBufferBindPipeline(primary, backwardPipeline);
		commandBufferPipelineBarriers2(primary, b3);
		commandBufferDispatch(primary, UVec3(width, height, 1u));

		bufferCopyToBuffer2(primary, buffer2, buffer3,
							A::NONE, A::NONE,
							S::COMPUTE_SHADER_BIT, S::BOTTOM_OF_PIPE_BIT);

		// assume that command buffer recording will be finished and submitted
	};

	// 1. pipeline copies buffer0 to buffer1
	// 2. compute copies buffer1 to storage image
	// 3. renderpass samples storage image to framebuffer
	// 4. compute copies framebuffer to buffer2
	// 5: pipeline copies buffer2 to buffer3
	
	std::vector<uint32_t> buffer0data(lm.getBindingElementCount(buffer0binding));
	std::iota(buffer0data.begin(), buffer0data.end(), 1u);
	lm.writeBinding(buffer0binding, buffer0data);

	std::vector<uint32_t> buffer1data(lm.getBindingElementCount(buffer1binding));
	std::iota(buffer1data.begin(), buffer1data.end(), 2u);
	lm.writeBinding(buffer1binding, buffer1data);

	std::vector<uint32_t> buffer2data(lm.getBindingElementCount(buffer2binding));
	std::iota(buffer2data.begin(), buffer2data.end(), 3u);
	lm.writeBinding(buffer2binding, buffer2data);

	std::vector<uint32_t> buffer3data(lm.getBindingElementCount(buffer3binding));
	std::iota(buffer3data.begin(), buffer3data.end(), 4u);
	lm.writeBinding(buffer3binding, buffer3data);

	ZCommandPool graphicsCommandPool = ctx.createGraphicsCommandPool();

	execute(ctx, graphicsCommandPool, beginEndCommand<decltype(record), decltype(draw)>, record, draw, framebuffer, renderPass);

	lm.readBinding(buffer1binding, buffer1data);
	lm.readBinding(buffer2binding, buffer2data);
	lm.readBinding(buffer3binding, buffer3data);

	uint32_t mismatchIndex = INVALID_UINT32;
	for (uint32_t i = 0; i < data_count(buffer0data); ++i)
	{
		if (buffer3data.at(i) != buffer2data.at(i) ||
			buffer2data.at(i) != buffer1data.at(i) ||
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
			  << ", expected: " << UVec4(buffer0data.at(mismatchIndex),
										 buffer0data.at(mismatchIndex),
										 buffer0data.at(mismatchIndex),
										 buffer0data.at(mismatchIndex))
			  << " got: " << UVec4(buffer0data.at(mismatchIndex),
								   buffer1data.at(mismatchIndex),
								   buffer2data.at(mismatchIndex),
								   buffer3data.at(mismatchIndex)) << std::endl;
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

