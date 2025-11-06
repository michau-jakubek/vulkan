#include "templateTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfVertexInput.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZCommandBuffer.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	bool buildAlways = false;
	Params(add_cref<std::string> assets_)
		: assets(assets_)
	{
	}
	OptionParser<Params> getParser();
};
constexpr Option optionBuildAlways{ "--build-always", 0 };
OptionParser<Params> Params::getParser()
{
	OptionFlags				flagsDef(OptionFlag::PrintValueAsDefault);
	add_ref<Params>			params = *this;
	OptionParser<Params>	parser(params);

	parser.addOption(&Params::buildAlways, optionBuildAlways,
		"Rebuild the shaders each time you run application", { false }, flagsDef)
		->setParamName("buildAlways");

	return parser;
}

TriLogicInt runTests(add_ref<Canvas> cs, add_cref<Params> params);
TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);
	OptionParser<Params> parser = params.getParser();
	parser.parse(cmdLineParams);
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

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
			.checkSupported("synchronization2");
		caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, gf.apiVer);

	return runTests(cs, params);
}

void prepareVertices(add_ref<VertexInput> vertexInput, add_cref<Params>)
{
	std::vector<Vec4>		vertices{ { -1, +1, 0, 1 }, { 0, -1, 0, 1 }, { +1, +1, 0, 1 } };
	vertexInput.binding(0).addAttributes(vertices);
}

std::tuple<ZShaderModule, ZShaderModule> buildProgram(ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "vertex.shader");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment.shader");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer,
		flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);

	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT)
	};
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface>	di(canvas.device.getInterface());
	LayoutManager				pl(canvas.device);
	VertexInput					vertexInput(canvas.device);
	prepareVertices(vertexInput, params);

	ZShaderModule				vertex;
	ZShaderModule				fragment;
	std::tie(vertex, fragment)	= buildProgram(canvas.device, params);

	const VkFormat				format = canvas.surfaceFormat;
	const VkClearValue			clearColor{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const std::vector<RPA>		colors{ RPA(AttachmentDesc::Presentation, format, clearColor,
															VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
	const ZAttachmentPool		attachmentPool(colors);
	const ZSubpassDescription2	subpass({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZRenderPass					renderPass = createRenderPass(canvas.device, attachmentPool, subpass);
	ZPipelineLayout				pipelineLayout = pl.createPipelineLayout();
	ZPipeline					pipeline = createGraphicsPipeline(pipelineLayout, renderPass,
																	vertexInput, vertex, fragment,
																	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	int drawTrigger = 1;
	canvas.events().setDefault(drawTrigger);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> sc,
		ZCommandBuffer cmd, ZFramebuffer fb)
	{
		commandBufferBegin(cmd);
		commandBufferBindPipeline(cmd, pipeline);
		commandBufferBindVertexBuffers(cmd, vertexInput);
		di.vkCmdSetViewport(*cmd, 0, 1, &sc.viewport);
		di.vkCmdSetScissor(*cmd, 0, 1, &sc.scissor);
		auto rpbi = commandBufferBeginRenderPass(cmd, fb);
		di.vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 1, 0, 0);
		commandBufferEndRenderPass(rpbi);
		commandBufferEnd(cmd);
	};

	return canvas.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<TEMPLATE_TEST>
{
	static bool record(TestRecord&);
};
bool TestRecorder<TEMPLATE_TEST>::record(TestRecord& record)
{
	record.name = "template_test";
	record.call = &prepareTests;
	return true;
}
