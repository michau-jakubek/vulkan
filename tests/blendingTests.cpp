#include "blendingTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCopyUtils.hpp"

#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace
{

using namespace vtf;
using ostream_ref = add_ref<std::ostream>;

struct TestParams
{
			TestParams	(add_cref<std::string> assets_);
	auto	getParser	(bool includeHelp, bool includeFile) -> OptionParser<TestParams>;
	void	print		(ostream_ref log, bool availableDualSourceBlend) const;
	auto	parseEnum	(add_cref<std::string>			text,
						add_cref<OptionT<uint32_t>>		sender,
						add_ref<bool>					status,
						add_ref<OptionParserState>		state,
						add_cref<std::map<uint32_t,		std::string>>	map) const -> uint32_t;
	auto	formatEnum	(add_cref<OptionT<uint32_t>>	sender,
						add_cref<std::map<uint32_t, std::string>>	map) const -> std::string;
	bool	operator==	(add_cref<TestParams> other) const;

	add_cref<std::string>	assets;
	std::string				file;
	VkExtent2D				extent;
	Vec4					constColor;
	Vec4					bkgColor;
	Vec4					color0;
	Vec4					color1;
	uint32_t				colorOp;
	uint32_t				alphaOp;
	uint32_t				srcColorFactor;
	uint32_t				dstColorFactor;
	uint32_t				srcAlphaFactor;
	uint32_t				dstAlphaFactor;
	bool					enableDualSrcBlending;

	static inline const std::map<uint32_t, std::string> mapVkBlendOp
	{
		{ VK_BLEND_OP_ADD,				"VK_BLEND_OP_ADD" },
		{ VK_BLEND_OP_SUBTRACT,			"VK_BLEND_OP_SUBTRACT" },
		{ VK_BLEND_OP_REVERSE_SUBTRACT,	"VK_BLEND_OP_REVERSE_SUBTRACT" },
		{ VK_BLEND_OP_MIN,				"VK_BLEND_OP_MIN" },
		{ VK_BLEND_OP_MAX,				"VK_BLEND_OP_MAX" },
	};

	static inline const std::map<uint32_t, std::string> mapVkBlendFactor
	{
		{ VK_BLEND_FACTOR_ZERO						, "VK_BLEND_FACTOR_ZERO" },
		{ VK_BLEND_FACTOR_ONE						, "VK_BLEND_FACTOR_ONE" },
		{ VK_BLEND_FACTOR_SRC_COLOR					, "VK_BLEND_FACTOR_SRC_COLOR" },
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR		, "VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR" },
		{ VK_BLEND_FACTOR_DST_COLOR					, "VK_BLEND_FACTOR_DST_COLOR" },
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR		, "VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR" },
		{ VK_BLEND_FACTOR_SRC_ALPHA					, "VK_BLEND_FACTOR_SRC_ALPHA" },
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA		, "VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA" },
		{ VK_BLEND_FACTOR_DST_ALPHA					, "VK_BLEND_FACTOR_DST_ALPHA" },
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA		, "VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA" },
		{ VK_BLEND_FACTOR_CONSTANT_COLOR			, "VK_BLEND_FACTOR_CONSTANT_COLOR" },
		{ VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR	, "VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR" },
		{ VK_BLEND_FACTOR_CONSTANT_ALPHA			, "VK_BLEND_FACTOR_CONSTANT_ALPHA" },
		{ VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA	, "VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA" },
		{ VK_BLEND_FACTOR_SRC_ALPHA_SATURATE		, "VK_BLEND_FACTOR_SRC_ALPHA_SATURATE" },
		{ VK_BLEND_FACTOR_SRC1_COLOR				, "VK_BLEND_FACTOR_SRC1_COLOR" },
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR		, "VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR" },
		{ VK_BLEND_FACTOR_SRC1_ALPHA				, "VK_BLEND_FACTOR_SRC1_ALPHA" },
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA		, "VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA" },
	};

	inline static const VkExtent2D defaultExtent = makeExtent2D(256, 256);
	inline static const Vec4 defaultBkgColor = Vec4(0, 1, 0, 0.6);
};
typedef std::tuple<TestParams, OptionParserState, std::string> TestParamsState;

TestParams::TestParams (add_cref<std::string> assets_)
	: assets				(assets_)
	, file					()
	, extent				(defaultExtent)
	, constColor			()
	, bkgColor				(defaultBkgColor)
	, color0				(1, 0, 0, 0.6)
	, color1				(0, 1, 0, 0.6)
	, colorOp				(VK_BLEND_OP_ADD)
	, alphaOp				(VK_BLEND_OP_ADD)
	, srcColorFactor		(VK_BLEND_FACTOR_SRC_ALPHA)
	, dstColorFactor		(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
	, srcAlphaFactor		(VK_BLEND_FACTOR_ONE)
	, dstAlphaFactor		(VK_BLEND_FACTOR_ZERO)
	, enableDualSrcBlending	(false)
{
}
void TestParams::print (add_ref<std::ostream> log, bool availableDualSourceBlend) const
{
	typedef routine_arg_t<decltype(std::setw), 0> sted_setw_0;
	TestParams	params	(*this);
	auto		parser	= params.getParser(true, true);
	const sted_setw_0 w = static_cast<sted_setw_0>(std::max(24u, parser.getMaxOptionNameLength()));

	log << std::left << std::setw(w) << "Extent" << UVec2(extent.width, extent.height) << std::endl;
	log << std::left << std::setw(w) << "dualSrcBlend" << boolean(availableDualSourceBlend) << std::endl;

	for (auto opt : parser.getOptions())
	{
		log << std::left << std::setw(w) << opt->getName() << opt->valueWriter() << std::endl;
	}
}

struct EventData
{
	uint32_t	testCount;
	uint32_t	testCounter;
	int			swapTrigger;
	EventData (uint32_t testCount_) : testCount(testCount_), testCounter(0u), swapTrigger(1) {}
};
void onKey (add_ref<Canvas> cs, void* userData, const int key, int, int action, int)
{
	auto ev = reinterpret_cast<add_ptr<EventData>>(userData);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		if (ev->testCount == (ev->testCounter + 1u))
			glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
		else
		{
			ev->testCounter += 1u;
			ev->swapTrigger += 1;
		}
	}
}
void onResize (add_ref<Canvas>, void* userData, int, int)
{
	auto ev = reinterpret_cast<add_ptr<EventData>>(userData);
	ev->swapTrigger += 1;
}

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
TriLogicInt runTests (add_ref<Canvas> ctx, add_cref<std::string> assets,
						add_cref<std::vector<TestParamsState>> set, bool fromFile, bool availableDualSourceBlend);

bool TestParams::operator==	(add_cref<TestParams> other) const
{
	return	extent.width				== other.extent.width
			&& extent.height			== other.extent.height
			&& constColor				== other.constColor
			&& bkgColor					== other.bkgColor
			&& color0					== other.color0
			&& color1					== other.color1
			&& colorOp					== other.colorOp
			&& alphaOp					== other.alphaOp
			&& srcColorFactor			== other.srcColorFactor
			&& dstColorFactor			== other.dstColorFactor
			&& srcAlphaFactor			== other.srcAlphaFactor
			&& dstAlphaFactor			== other.dstAlphaFactor
			&& enableDualSrcBlending	== other.enableDualSrcBlending;
}

std::string TestParams::formatEnum (add_cref<OptionT<uint32_t>> sender, add_cref<std::map<uint32_t, std::string>> map) const
{
	return map.at(sender.m_storage);
}

uint32_t TestParams::parseEnum	(add_cref<std::string>						text,
								add_cref<OptionT<uint32_t>>					sender,
								add_ref<bool>								status,
								add_ref<OptionParserState>					state,
								add_cref<std::map<uint32_t, std::string>>	map) const
{
	uint32_t mc = 0u;
	uint32_t bm = INVALID_UINT32;
	const std::string upperText = toUpper(text);
	const std::string_view svText(upperText);
	for (add_cref<std::pair<const uint32_t, std::string>> item : map)
	{
		if (auto k = std::string_view(item.second).find(svText); k != text.npos)
		{
			bm = item.first;
			mc += 1u;
		}
	}
	if (mc != 1u)
	{
		status = false;
		state.hasWarnings = true;

		if (mc == 0u)
			state.messages << "WARNING: Unable to parse " << std::quoted(sender.getName()) << " from " << std::quoted(text);
		else
			state.messages << "WARNING: Given " << std::quoted(text) << " for " << std::quoted(sender.getName()) << " is ambigous";

		state.messages << ", default value " << sender.defaultWriter() << " was used" << std::endl;
	}
	else
	{
		status = true;
	}

	return bm;
}
constexpr Option optionFile("-file", 1);
constexpr Option optionDualSource("-dual-source", 0);
constexpr Option optionConstColor("-const-color", 1);
constexpr Option optionBkgColor("-bkg-color", 1);
constexpr Option optionColor0("-color0", 1);
constexpr Option optionColor1("-color1", 1);
constexpr Option optionColorOp("-color-op", 1);
constexpr Option optionAlphaOp("-alpha-op", 1);
constexpr Option optionSrcColorFactor("-src-color-factor", 1);
constexpr Option optionDstColorFactor("-dst-color-factor", 1);
constexpr Option optionSrcAlphaFactor("-src-alpha-factor", 1);
constexpr Option optionDstAlphaFactor("-dst-alpha-factor", 1);
OptionParser<TestParams> TestParams::getParser (bool includeHelp, bool includeFile)
{
	typename OptionT<uint32_t>::format_cb formatVkBlendOp =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendOp);
	typename OptionT<uint32_t>::format_cb formatVkBlendFactor =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendFactor);
	typename OptionT<uint32_t>::parse_cb parseVkBlendOp =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
					std::placeholders::_3, std::placeholders::_4, mapVkBlendOp);
	typename OptionT<uint32_t>::parse_cb parseVkBlendFactor =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, mapVkBlendFactor);
	add_cptr<char> vec4TypeName = "vec4";
	add_cptr<char> textTypeName = "text";

	OptionFlags			flags	(OptionFlag::PrintDefault);
	add_ref<TestParams> params	= *this;
	OptionParser<TestParams>	parser(params, includeHelp);

	if (includeFile)
	{
		add_cptr<char> desc =
			"Define the tests set. If specified then all below options will "
			"be read from <file> instead of command line. Each of line define "
			"separate test. Please refer to an example in assets directory.";
		auto optFile = parser.addOption(&TestParams::file, optionFile, desc);
		optFile->setTypeName("file");
	}
	parser.addOption(&TestParams::enableDualSrcBlending, optionDualSource, "Enable dual source blending.");
	parser.addOption(&TestParams::constColor, optionConstColor, "Const color.", { params.constColor }, flags)->setTypeName(vec4TypeName);
	parser.addOption(&TestParams::bkgColor, optionBkgColor, "Background color.", { params.bkgColor }, flags)->setTypeName(vec4TypeName);
	parser.addOption(&TestParams::color0, optionColor0, "Blending color 0.", { params.color0 }, flags)->setTypeName(vec4TypeName);
	parser.addOption(&TestParams::color1, optionColor1, "Blending color 1.", { params.color1 }, flags)->setTypeName(vec4TypeName);

	{
		auto optVkBlendOp = parser.addOption(&TestParams::colorOp, optionColorOp, "Color blend op.",
			{ params.colorOp }, flags, parseVkBlendOp, formatVkBlendOp);
		optVkBlendOp->setTypeName(textTypeName);
		optVkBlendOp->setDefault(TestParams::mapVkBlendOp.at(params.colorOp));

		optVkBlendOp = parser.addOption(&TestParams::alphaOp, optionAlphaOp, "Alpha blend op.",
			{ params.alphaOp }, flags, parseVkBlendOp, formatVkBlendOp);
		optVkBlendOp->setTypeName(textTypeName);
		optVkBlendOp->setDefault(TestParams::mapVkBlendOp.at(params.alphaOp));
	}
	{
		auto optVkBlendFactor = parser.addOption(&TestParams::srcColorFactor, optionSrcColorFactor, "Source color blend factor.",
			{ params.srcColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor);
		optVkBlendFactor->setTypeName(textTypeName);
		optVkBlendFactor->setDefault(TestParams::mapVkBlendFactor.at(params.srcColorFactor));

		optVkBlendFactor = parser.addOption(&TestParams::dstColorFactor, optionDstColorFactor, "Destination color blend factor.",
			{ params.dstColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor);
		optVkBlendFactor->setTypeName(textTypeName);
		optVkBlendFactor->setDefault(TestParams::mapVkBlendFactor.at(params.dstColorFactor));

		optVkBlendFactor = parser.addOption(&TestParams::srcAlphaFactor, optionSrcAlphaFactor, "Source alpha blend factor.",
			{ params.srcAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor);
		optVkBlendFactor->setTypeName(textTypeName);
		optVkBlendFactor->setDefault(TestParams::mapVkBlendFactor.at(params.srcAlphaFactor));

		optVkBlendFactor = parser.addOption(&TestParams::dstAlphaFactor, optionDstAlphaFactor, "Destination alpha blend factor.",
			{ params.dstAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor);
		optVkBlendFactor->setTypeName(textTypeName);
		optVkBlendFactor->setDefault(TestParams::mapVkBlendFactor.at(params.dstAlphaFactor));
	}

	return parser;
}
uint32_t processFile (add_cref<fs::path> file, add_cref<std::string> assets, add_ref<std::vector<TestParamsState>> set)
{
	uint32_t		itemCount	= 0;
	std::ifstream	stream		(file);
	if (stream.is_open())
	{
		std::string line;
		while (std::getline(stream, line))
		{
			std::istringstream iss(line);

			std::string item;
			iss >> std::quoted(item);
			if (item.empty() || startswith(item, '#'))
			{
				continue;
			}

			strings cmdLineParams { std::move(item) };
			while (iss.tellg() >= 0)
			{
				iss >> std::quoted(item);
				cmdLineParams.emplace_back(std::move(item));
			}

			TestParams					params	(assets);
			OptionParser<TestParams>	parser	(params.getParser(false, false));

			parser.parse(cmdLineParams);
			OptionParserState			state	= parser.getState();

			set.emplace_back(std::move(params), std::move(state), line);
			itemCount = itemCount + 1u;
		}
	}
	return itemCount;
}
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	bool							fromFile	(false);
	TestParams						params		(record.assets);
	std::vector<TestParamsState>	set			{ std::make_tuple(params, OptionParserState(), std::string()) };

	{
		add_ref<OptionParserState>	state	(std::get<1>(set.at(0)));
		OptionParser<TestParams>	parser	= params.getParser(true, true);
		parser.parse(cmdLineParams);
		state = parser.getState();

		if (state.hasHelp)
		{
			std::cout	<< "Navigation:\n"
							"  Esc   - quit this app immediately.\n"
							"  Space - switch to the subsequent test.\n"
							"Parameters:\n";
			parser.printOptions(std::cout, 70);
			return {};
		}

		if (state.hasErrors || state.hasWarnings)
		{
			std::cout << state.messages.str() << std::endl;
			if (state.hasErrors) return {};
		}

		auto optFile = parser.getOptionByName(optionFile.name);
		if (optFile && optFile->getTouched())
		{
			fromFile = true;
			const fs::path path(params.file);
			if (!(fs::exists(path) && !fs::is_directory(path)))
			{
				std::cout << "ERROR: Specified file " << path << " does not exist or is it a directory" << std::endl;
				return {};
			}
			set.clear();
			if (const uint32_t testCount = processFile(path, record.assets, set); testCount == 0u)
			{
				std::cout << "WARNING: No tests to execute in " << path << std::endl;
				return {};
			}
		}
	}

	add_cref<GlobalAppFlags>	appFlags			= getGlobalAppFlags();
	CanvasStyle					canvasStyle			= Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	bool availableDualSourceBlend = false;
	VkPhysicalDeviceFeatures2	requiredfeatures	= makeVkStruct();
	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		UNREF(extensions);
		VkPhysicalDeviceFeatures2 availableFatures = deviceGetPhysicalFeatures2(physicalDevice);
		requiredfeatures.features.dualSrcBlend = availableFatures.features.dualSrcBlend;
		availableDualSourceBlend = availableFatures.features.dualSrcBlend != VK_FALSE;
		return requiredfeatures;
	};

	Canvas cs(record.name, appFlags.layers, strings(), strings(), canvasStyle, onEnablingFeatures, Version(1, 3));

	return runTests(cs, record.assets, set, fromFile, availableDualSourceBlend);
}

std::array<ZShaderModule, 5> buildProgram (ZDevice device, add_cref<std::string> assets, bool availableDualSourceBlend)
{
	const GlobalAppFlags	flags			(getGlobalAppFlags());
	ProgramCollection		program			(device, assets);

	const std::string		vertexName		("shader.vert");
	const std::string		bkgFragName		("background.frag");
	const std::string		singleFragName	("single.frag");
	const std::string		dualFragName	("dual.frag");

	program.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertexName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, bkgFragName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, singleFragName);
	if (availableDualSourceBlend)
	{
		program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragName);
	}
	program.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, false);

	ZShaderModule	vertex		= program.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule	bkgFrag		= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	ZShaderModule	singleFrag	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	ZShaderModule	dualFrag;	

	vertex.name(vertexName);
	bkgFrag.name(bkgFragName);
	singleFrag.name(singleFragName);
	if (availableDualSourceBlend)
	{
		dualFrag = program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 2);
		dualFrag.name(dualFragName);
	}

	return
	{
		vertex,
		bkgFrag,
		singleFrag,
		dualFrag
	};
}

ZBuffer genVertices (add_ref<VertexInput> vi)
{
	const std::vector<Vec2> vertices
	{
		{ -0.75, +0.50 }, { -0.75, -0.75 }, { +0.50, -0.75 },
		{ +0.50, -0.75 }, { +0.50, +0.50 }, { -0.75, +0.50 },
		{ -0.50, +0.75 }, { -0.50, -0.50 }, { +0.75, -0.50 },
		{ +0.75, -0.50 }, { +0.75, +0.75 }, { -0.50, +0.75 },
	};
	vi.binding(0).addAttributes(vertices);
	return vi.binding(0).getBuffer();
}

ZPipeline makeBlendPipeline	(add_cref<TestParamsState>	test,
							ZPipelineLayout				layout,
							ZRenderPass					renderPass,
							add_cref<VertexInput>		vertices,
							ZShaderModule				vertex,
							ZShaderModule				singleFragment,
							ZShaderModule				dualFragment,
							bool						availableDualSourceBlend)
{
	add_cref<TestParams>	params(std::get<0>(test));
	std::cout << std::get<1>(test).messagesText();
	if (params.enableDualSrcBlending && !availableDualSourceBlend)
	{
		std::cout << "ERROR: Dual source blending not supportd by device" << std::endl;
		return {};
	}

	VkPipelineColorBlendAttachmentState s = gpp::defaultBlendAttachmentState;
	s.blendEnable = VK_TRUE;
	s.colorBlendOp = VkBlendOp(params.colorOp);
	s.alphaBlendOp = VkBlendOp(params.alphaOp);
	s.srcColorBlendFactor = VkBlendFactor(params.srcColorFactor);
	s.dstColorBlendFactor = VkBlendFactor(params.dstColorFactor);
	s.srcAlphaBlendFactor = VkBlendFactor(params.srcAlphaFactor);
	s.dstAlphaBlendFactor = VkBlendFactor(params.dstAlphaFactor);

	ZShaderModule blendFragment = params.enableDualSrcBlending ? dualFragment : singleFragment;
	return createGraphicsPipeline(layout, renderPass, vertices, vertex, blendFragment, TestParams::defaultExtent,
									gpp::BlendAttachmentState(std::make_pair(0u, s)),
									gpp::BlendConstants(params.constColor));
}
TriLogicInt runTests (add_ref<Canvas> ctx, add_cref<std::string> assets,
						add_cref<std::vector<TestParamsState>> set, bool fromFile, bool availableDualSourceBlend)
{
	std::array<ZShaderModule, 5> shaders	= buildProgram(ctx.device, assets, availableDualSourceBlend);
	ZShaderModule				commonVert	= shaders.at(0);
	ZShaderModule				bkgFrag		= shaders.at(1);
	ZShaderModule				singleFrag	= shaders.at(2);
	ZShaderModule				dualFrag	= shaders.at(3);

	VertexInput					vertices	(ctx.device);
	genVertices(vertices);
	const uint32_t				vertexCount = vertices.getVertexCount(0);

	const VkFormat				colorFormat		= VK_FORMAT_R32G32B32A32_SFLOAT;
	struct PushConstant
	{
		Vec4 background;
		Vec4 inColor0;
		Vec4 inColor1;
	}							pc;

	ZRenderPass					colorRenderPass	= createColorRenderPass(ctx.device, { colorFormat });
	ZImage						colorImage		= ctx.createColorImage2D(colorFormat, TestParams::defaultExtent);

	ZImageView					colorView		= createImageView(colorImage);
	ZFramebuffer				colorFB			= createFramebuffer(colorRenderPass, TestParams::defaultExtent, { colorView });

	LayoutManager				lm				(ctx.device);
	ZPipelineLayout				colorLayout		= lm.createPipelineLayout(ZPushRange<PushConstant>());
	ZPipeline					colorPipeline	= createGraphicsPipeline(colorLayout, colorRenderPass,
																		TestParams::defaultExtent, vertices,
																		commonVert, bkgFrag);
	ZPipeline					blendPipeline;

	EventData ev(data_count(set));
	ctx.events().cbKey.set(onKey, &ev);
	ctx.events().cbWindowSize.set(onResize, &ev);
	uint32_t lastTextIndex = INVALID_UINT32;

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>,
								ZCommandBuffer displayCmd, ZFramebuffer displayFB)
	{
		add_cref<TestParamsState>	test	(set.at(ev.testCounter));
		add_cref<TestParams>		params	(std::get<0>(test));

		if (lastTextIndex != ev.testCounter)
		{
			lastTextIndex = ev.testCounter;
			if (fromFile)
			{
				std::cout << "==============" << std::endl;
				std::cout << "Perform test: " << ev.testCounter << std::endl;
				std::cout << "Command line: " << std::get<2>(test) << std::endl;
			}
			params.print(std::cout, availableDualSourceBlend);
		}

		if ((ev.testCounter == 0u && !blendPipeline.has_handle())
			|| (ev.testCounter > 0u && !(params == std::get<0>(set.at(ev.testCounter - 1u)))))
		{
			blendPipeline = makeBlendPipeline(test, colorLayout, colorRenderPass, vertices,
												commonVert, singleFrag, dualFrag, availableDualSourceBlend);
		}

		pc.background	= blendPipeline.has_handle() ? params.bkgColor : TestParams::defaultBkgColor;
		pc.inColor0		= params.color0;
		pc.inColor1		= params.color1;

		ZImage				displayImage	= framebufferGetImage(displayFB);
		ZImageMemoryBarrier	srcPreBlit		(colorImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstPreBlit		(displayImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(displayCmd);
			commandBufferBindVertexBuffers(displayCmd, vertices);
			commandBufferPushConstants(displayCmd, colorLayout, pc);
			{
				commandBufferBindPipeline(displayCmd, colorPipeline);
				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
				vkCmdDraw(*displayCmd, (vertexCount / 2u), 1, 0, 0);
				commandBufferEndRenderPass(rpbi);
			}
			if (blendPipeline.has_handle())
			{
				commandBufferBindPipeline(displayCmd, blendPipeline);
				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
				vkCmdDraw(*displayCmd, (vertexCount / 2u), 1, (vertexCount / 2u), 0);
				commandBufferEndRenderPass(rpbi);
			}
			commandBufferPipelineBarriers(displayCmd,
											VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											srcPreBlit, dstPreBlit);
			commandBufferBlitImage(displayCmd, colorImage, displayImage, VK_FILTER_NEAREST);
			commandBufferMakeImagePresentationReady(displayCmd, displayImage);
		commandBufferEnd(displayCmd);
	};

	ZRenderPass swapRenderPass = createColorRenderPass(ctx.device, { ctx.surfaceFormat });

	return ctx.run(onCommandRecording, swapRenderPass, std::ref(ev.swapTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<BLENDING>
{
	static bool record(TestRecord&);
};
bool TestRecorder<BLENDING>::record (TestRecord& record)
{
	record.name = "blending";
	record.call = &prepareTests;
	return true;
}


