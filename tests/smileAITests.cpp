#include "smileAITests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZRenderPass.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	std::string cacheFileName;
	Params(add_cref<std::string> assets_)
		: assets(assets_)
		, cacheFileName("pipeline.cache") {}
};

TriLogicInt prepareTests(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine);
TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	UNREF(cmdLine);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		UNREF(caps);
	};
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, gf.apiVer);
	return runTests(cs, Params(record.assets));
}

std::array<ZShaderModule, 2> genShaders(ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);
	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT)
	};
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface>	di			= canvas.device.getInterface();

	VertexInput					vertexInput(canvas.device);
	vertexInput.binding(0).addAttributes(VertexInput::fullQuad<Vec2>());

	const auto					[vs, fs]	= genShaders(canvas.device, params);

	ZCommandPool				cmdPool		= createCommandPool(canvas.device, canvas.graphicsQueue);
	LayoutManager				pl			(canvas.device, cmdPool);

	struct PushConstant			{ Vec2 iResolution; };
	ZPushConstants				pushConstants(ZPushRange<PushConstant>(shaderGetStage(fs)));

	const VkFormat				format	= canvas.surfaceFormat;
	const VkClearValue			clearColor	{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const std::vector<RPA>		colors		{ RPA(AttachmentDesc::Presentation, format, clearColor,
															VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
	const ZAttachmentPool		attachmentPool(colors);
	const ZSubpassDescription2	subpass		({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZRenderPass					renderPass	= createRenderPass(canvas.device, attachmentPool, subpass);
	ZPipelineLayout				pipelineLayout	= pl.createPipelineLayout(pushConstants);
	ZPipelineCache				pipelineCache	= createPipelineCache(canvas.device, params.cacheFileName, true, false);
	ZPipeline					pipeline		= createGraphicsPipeline(pipelineLayout, pipelineCache,
																	renderPass, vertexInput, vs, fs,
																	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	pipelineCache.saveToFile();

	int drawTrigger = 1;
	canvas.events().setDefault(drawTrigger);

	auto onCommandRecording = [&](add_ref<Canvas> cs, add_cref<Canvas::Swapchain> swapchain,
									ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		const PushConstant pc{ Vec2(cs.width, cs.height) };
		commandBufferBegin(cmdBuffer);
		commandBufferBindPipeline(cmdBuffer, pipeline);
		commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
		commandBufferSetViewportAndScissor(cmdBuffer, swapchain);
		commandBufferPushConstants(cmdBuffer, pipelineLayout, pc);
		auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);
            VTF_CALL_CHECK(di.vkCmdDraw, *cmdBuffer, vertexInput.getVertexCount(0), 1u, 0u, 0u);
		commandBufferEndRenderPass(rpbi);
		commandBufferEnd(cmdBuffer);
	};

	return canvas.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<SMILE_AI>
{
	static bool record(TestRecord&);
};
bool TestRecorder<SMILE_AI>::record(TestRecord& record)
{
	record.name = "smile_ai";
	record.call = &prepareTests;
	return true;
}
