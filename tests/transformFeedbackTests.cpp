#include "transformFeedbackTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfVertexInput.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
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

		caps.addUpdateFeatureIf(&VkPhysicalDeviceTransformFeedbackFeaturesEXT::transformFeedback)
			.checkSupported("transformFeedback");
		caps.addExtension(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME).checkSupported();
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, gf.apiVer);

	return runTests(cs, params);
}

void prepareVertices(add_ref<VertexInput> vertexInput, add_cref<Params>)
{
	std::vector<Vec2>		vertices{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
	std::vector<Vec3>		colors{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	vertexInput.binding(0).addAttributes(vertices, colors);
}

std::tuple<ZShaderModule, ZShaderModule> buildProgram(ZDevice device, add_cref<Params> params)
{
	ProgramCollection p(device, params.assets);
	p.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	p.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);
	ZShaderModule				vertShaderModule = p.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	return { vertShaderModule, ZShaderModule() };
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface> di = canvas.device.getInterface();

	VertexInput vertexInput(canvas.device);
	prepareVertices(vertexInput, params);
	auto [vertexModule, otherModule] = buildProgram(canvas.device, params);
	ZBuffer  buf = createBuffer(canvas.device, 1024u, ZBufferUsageFlags(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT));
	VkDeviceSize off = 0u;
	VkDeviceSize siz = bufferGetSize(buf);
	ZCommandPool cmdPool = createCommandPool(canvas.device, canvas.graphicsQueue);
	LayoutManager lm(canvas.device, cmdPool);
	ZPipelineLayout pipelineLayout = lm.createPipelineLayout();
	ZPipeline pipeline = createGraphicsPipeline(pipelineLayout, vertexInput, vertexModule, gpp::RasterizerDiscardEnable(false));

	//VkPipelineRasterizationStateStreamCreateInfoEXT


	//{
	//	OneShotCommandBuffer cmd(cmdPool);
	//	commandBufferBindVertexBuffers(cmd, vertexInput);
	//	commandBufferBindPipeline(cmd, pipeline);
	//	di.vkCmdBindTransformFeedbackBuffersEXT(**cmd, 0u, 1u, buf.ptr(), &off, &siz);
	//	di.vkCmdBeginTransformFeedbackEXT(**cmd, 0u, 0u, nullptr, nullptr);
	//	di.vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1u, 0u, 0u);
	//	di.vkCmdEndTransformFeedbackEXT(**cmd, 0u, 0u, nullptr, nullptr);
	//}

	return {};
}

} // unnamed namespace

template<> struct TestRecorder<TRANSFORM_FEEDBACK>
{
	static bool record(TestRecord&);
};
bool TestRecorder<TRANSFORM_FEEDBACK>::record(TestRecord& record)
{
	record.name = "transform_feedback";
	record.call = &prepareTests;
	return true;
}
