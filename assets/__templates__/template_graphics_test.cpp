#include "transformFeedbackTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfVertexInput.hpp"

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
}

std::tuple<ZShaderModule, ZShaderModule> buildProgram(ZDevice device, add_cref<Params>)
{
	return {};
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
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
