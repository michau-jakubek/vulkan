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
			TestParams	(ZPhysicalDevice physicalDevice, add_cref<std::string> assets_);
	auto	getParser	(bool includeHelp, bool includeFile) -> OptionParser<TestParams>;
	void	print		(ostream_ref log, bool availableDualSourceBlend) const;
	void	printOps	(ostream_ref log) const;
	void	printFactors(ostream_ref log) const;
	void	printFormats(ostream_ref log) const;
	auto	parseEnum	(add_cref<std::string>			text,
						add_cref<OptionT<uint32_t>>		sender,
						add_ref<bool>					status,
						add_ref<OptionParserState>		state,
						add_cref<std::map<uint32_t,		std::string>>	map) const -> uint32_t;
	auto	formatEnum	(add_cref<OptionT<uint32_t>>	sender,
						add_cref<std::map<uint32_t, std::string>>	map) const -> std::string;
	bool	operator==	(add_cref<TestParams> other) const;

	ZPhysicalDevice			device;
	add_cref<std::string>	assets;
	std::string				file;
	VkExtent2D				extent;
	uint32_t				colorFormat;
	Vec4					constColor;
	Vec4					color0;
	Vec4					color1;
	uint32_t				colorOp;
	uint32_t				alphaOp;
	uint32_t				srcColorFactor;
	uint32_t				dstColorFactor;
	uint32_t				srcAlphaFactor;
	uint32_t				dstAlphaFactor;
	bool					enableDualSrcBlending;
	bool					printBlendOps;
	bool					printBlendFactors;
	bool					printColorFormats;

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

	inline static const VkExtent2D	defaultExtent		= makeExtent2D(256, 256);
	inline static const VkFormat	defaultColorFormat	= VK_FORMAT_R32G32B32A32_SFLOAT;
};
typedef std::tuple<TestParams, OptionParserState, std::string> TestParamsState;

bool doesFormatSupportBlending (ZPhysicalDevice device, VkFormat format)
{
	VkFormatProperties	properties{};
	const VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
										| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT
										| VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
										| VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
	vkGetPhysicalDeviceFormatProperties(*device, format, &properties);

	return ((properties.optimalTilingFeatures & features) == features);
}

TestParams::TestParams (ZPhysicalDevice physicalDevice, add_cref<std::string> assets_)
	: device				(physicalDevice)
	, assets				(assets_)
	, file					()
	, extent				(defaultExtent)
	, colorFormat			(defaultColorFormat)
	, constColor			()
	, color0				(1, 0, 0, 0.6)
	, color1				(0, 1, 0, 0.6)
	, colorOp				(VK_BLEND_OP_ADD)
	, alphaOp				(VK_BLEND_OP_ADD)
	, srcColorFactor		(VK_BLEND_FACTOR_SRC_ALPHA)
	, dstColorFactor		(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
	, srcAlphaFactor		(VK_BLEND_FACTOR_ONE)
	, dstAlphaFactor		(VK_BLEND_FACTOR_ZERO)
	, enableDualSrcBlending	(false)
	, printBlendOps			(false)
	, printBlendFactors		(false)
	, printColorFormats		(false)
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
void TestParams::printOps (ostream_ref log) const
{
	log << "  VkBlendOp  \n-------------\n";
	for (add_cref<std::pair<const uint32_t, std::string>> item : mapVkBlendOp)
	{
		log << "  " << item.second << std::endl;
	}
}
void TestParams::printFactors (ostream_ref log) const
{
	log << "  VkBlendFactor  \n-----------------\n";
	for (add_cref<std::pair<const uint32_t, std::string>> item : mapVkBlendFactor)
	{
		log << "  " << item.second << std::endl;
	}
}
void TestParams::printFormats (ostream_ref log) const
{
	log << "  VkFormat  \n------------\n";
	uint32_t j = 0u;
	ZFormatInfoIterator i;
	while (i.next())
	{
		if (doesFormatSupportBlending(device, i.format))
			log << "  " << (j++) << ": " << i.name << std::endl;
	}
}

struct EventData
{
	uint32_t	testCount;
	uint32_t	testCounter;
	int			swapTrigger;
	bool		refreshing;
	EventData (uint32_t testCount_) : testCount(testCount_), testCounter(0u), swapTrigger(1), refreshing(false) {}
};
void onKey (add_ref<Canvas> cs, void* userData, const int key, int, int action, int)
{
	auto ev = reinterpret_cast<add_ptr<EventData>>(userData);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	if ((key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER) && action == GLFW_PRESS)
	{
		if (ev->testCount == (ev->testCounter + 1u))
			glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
		else
		{
			ev->testCounter += 1u;
			ev->swapTrigger += 1;
		}
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		ev->refreshing = true;
		ev->swapTrigger += 1;
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
			&& colorFormat				== other.colorFormat
			&& constColor				== other.constColor
			&& colorOp					== other.colorOp
			&& alphaOp					== other.alphaOp
			&& srcColorFactor			== other.srcColorFactor
			&& dstColorFactor			== other.dstColorFactor
			&& srcAlphaFactor			== other.srcAlphaFactor
			&& dstAlphaFactor			== other.dstAlphaFactor
			&& enableDualSrcBlending	== other.enableDualSrcBlending;
}

constexpr Option optionPrintBlenOps("-print-blend-ops", 0);
constexpr Option optionPrintBlendFactors("-print-blend-factors", 0);
constexpr Option optionPrintColorFormats("-print-color-formats", 0);
constexpr Option optionFile("-file", 1);
constexpr Option optionDualSource("-dual-source", 0);
constexpr Option optionConstColor("-const-color", 1);
constexpr Option optionColor0("-color0", 1, __COUNTER__);
constexpr Option optionColor1("-color1", 1, __COUNTER__);
constexpr Option optionColorFormat("-format", 1, __COUNTER__);
constexpr Option optionColorOp("-color-op", 1, __COUNTER__);
constexpr Option optionAlphaOp("-alpha-op", 1, __COUNTER__);
constexpr Option optionSrcColorFactor("-src-color-factor", 1, __COUNTER__);
constexpr Option optionDstColorFactor("-dst-color-factor", 1, __COUNTER__);
constexpr Option optionSrcAlphaFactor("-src-alpha-factor", 1, __COUNTER__);
constexpr Option optionDstAlphaFactor("-dst-alpha-factor", 1, __COUNTER__);

std::string TestParams::formatEnum (
	add_cref<OptionT<uint32_t>> sender,
	add_cref<std::map<uint32_t, std::string>> map) const
{
	if (sender.id == optionColorFormat.id)
	{
		add_cptr<char> s = formatGetString(VkFormat(sender.m_storage));
		return s ? s : "VK_FORMAT_UNDEFINED";
	}
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

	std::map<uint32_t, std::string> formatMap;
	if (sender.id == optionColorFormat.id)
	{
		ZFormatInfoIterator	it;
		while (it.next())
		{
			if (doesFormatSupportBlending(device, it.format))
				formatMap[uint32_t(it.format)] = it.name;
		}
	}

	for (add_cref<std::pair<const uint32_t, std::string>> item
		: (sender.id == optionColorFormat.id) ? formatMap : map)
	{
		const std::string_view svItem(item.second);
		if (auto k = svItem.find(svText); k != text.npos)
		{
			bm = item.first;

			if (k + svText.length() != svItem.length())
				mc += 1u;
			else
			{
				mc = 1u;
				break;
			}

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

OptionParser<TestParams> TestParams::getParser (bool includeHelp, bool includeFile)
{
	typename OptionT<uint32_t>::format_cb formatVkBlendOp =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendOp);
	typename OptionT<uint32_t>::format_cb formatVkBlendFactor =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendFactor);
	typename OptionT<uint32_t>::format_cb formatVkFormat =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, std::map<uint32_t, std::string>());

	typename OptionT<uint32_t>::parse_cb parseVkBlendOp =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
					std::placeholders::_3, std::placeholders::_4, mapVkBlendOp);
	typename OptionT<uint32_t>::parse_cb parseVkBlendFactor =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, mapVkBlendFactor);
	typename OptionT<uint32_t>::parse_cb parseVkFormat =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, std::map<uint32_t, std::string>());

	add_cptr<char> vec4TypeName = "vec4";
	add_cptr<char> textTypeName = "text";

	OptionFlags			flags	(OptionFlag::PrintDefault);
	add_ref<TestParams> params	= *this;
	OptionParser<TestParams>	parser(params, includeHelp);

	if (includeHelp)
	{
		parser.addOption(&TestParams::printBlendOps, optionPrintBlenOps,
			"Print available VkBlendOp enum values", { params.printBlendOps }, flags);
		parser.addOption(&TestParams::printBlendFactors, optionPrintBlendFactors,
			"Print available VkBlendFactor enum values", { params.printBlendOps }, flags);
		parser.addOption(&TestParams::printColorFormats, optionPrintColorFormats,
			"Print available VkFormat enum values", { params.printColorFormats }, flags);
	}

	if (includeFile)
	{
		add_cptr<char> desc =
			"Define the tests set. If specified then all below options will "
			"be read from <file> instead of command line. Each of line define "
			"separate test. Please refer to an example in assets directory.";
		auto optFile = parser.addOption(&TestParams::file, optionFile, desc);
		optFile->setTypeName("file");
	}
	parser.addOption(&TestParams::enableDualSrcBlending, optionDualSource,
		"Enable dual source blending. Another shader with two outputs will be used.");
	parser.addOption(&TestParams::constColor, optionConstColor, "Const color", { params.constColor }, flags)->setTypeName(vec4TypeName);
	parser.addOption(&TestParams::color0, optionColor0, "Blending color 0", { params.color0 }, flags)->setTypeName(vec4TypeName);
	parser.addOption(&TestParams::color1, optionColor1, "Blending color 1", { params.color1 }, flags)->setTypeName(vec4TypeName);

	{
		auto optVkBlendOp = parser.addOption(&TestParams::colorOp, optionColorOp, "Color blend op",
			{ params.colorOp }, flags, parseVkBlendOp, formatVkBlendOp);
		optVkBlendOp->setTypeName(textTypeName);
		optVkBlendOp->setDefault(TestParams::mapVkBlendOp.at(params.colorOp));

		optVkBlendOp = parser.addOption(&TestParams::alphaOp, optionAlphaOp, "Alpha blend op",
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
	{
		auto optVkFormat = parser.addOption(&TestParams::colorFormat, optionColorFormat, "Blending color format",
			{ params.dstAlphaFactor }, flags, parseVkFormat, formatVkFormat);
		optVkFormat->setTypeName(textTypeName);		
		optVkFormat->setDefault(formatGetString(defaultColorFormat));
	}

	return parser;
}
uint32_t processFile (
	ZPhysicalDevice device,
	add_cref<fs::path> file,
	add_cref<std::string> assets,
	add_ref<std::vector<TestParamsState>> set)
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

			TestParams					params	(device, assets);
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
	add_cref<GlobalAppFlags>	appFlags		= getGlobalAppFlags();
	ZInstance					instance		= createInstance(
		record.name, getAllocationCallbacks(), appFlags.layers, strings(), Version(1, 3));
	ZPhysicalDevice				physicalDevice = selectPhysicalDevice(
		make_signed(appFlags.physicalDeviceIndex), instance, strings());

	bool							fromFile	(false);
	std::vector<TestParamsState>	set			{ std::make_tuple(TestParams(physicalDevice, record.assets),
													OptionParserState(), std::string()) };
	{
		add_ref<TestParams>			params	(std::get<0>(set.at(0)));
		add_ref<OptionParserState>	state	(std::get<1>(set.at(0)));
		OptionParser<TestParams>	parser	= params.getParser(true, true);
		parser.parse(cmdLineParams);
		state = parser.getState();

		if (state.hasHelp)
		{
			std::cout	<< "Navigation:\n"
							"  Esc         - quit this app immediately.\n"
							"  Space|Enter - switch to the subsequent test.\n"
							"  R           - refresh current view.\n"
							"Parameters:\n";
			parser.printOptions(std::cout, 70);
			return {};
		}

		if (params.printBlendOps)
		{
			params.printOps(std::cout);
			return {};
		}
		if (params.printBlendFactors)
		{
			params.printFactors(std::cout);
			return {};
		}
		if (params.printColorFormats)
		{
			params.printFormats(std::cout);
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
			if (const uint32_t testCount = processFile(physicalDevice, path, record.assets, set); testCount == 0u)
			{
				std::cout << "WARNING: No tests to execute in " << path << std::endl;
				return {};
			}
		}
	}

	CanvasStyle					canvasStyle			= Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	bool availableDualSourceBlend = false;
	VkPhysicalDeviceFeatures2	requiredfeatures	= makeVkStruct();
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		VkPhysicalDeviceFeatures2 availableFatures = deviceGetPhysicalFeatures2(caps.physicalDevice);
		requiredfeatures.features.dualSrcBlend = availableFatures.features.dualSrcBlend;
		availableDualSourceBlend = availableFatures.features.dualSrcBlend != VK_FALSE;
		return requiredfeatures;
	};

	Canvas cs(physicalDevice, canvasStyle, onEnablingFeatures, appFlags.debugPrintfEnabled);

	return runTests(cs, record.assets, set, fromFile, availableDualSourceBlend);
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule, ZShaderModule, ZShaderModule>
buildProgram (ZDevice device, add_cref<std::string> assets, bool availableDualSourceBlend)
{
	UNREF(availableDualSourceBlend);

	const GlobalAppFlags	flags			(getGlobalAppFlags());
	ProgramCollection		program			(device, assets);

	const std::string		vertexName			("common.vert");
	const std::string		singleFragFloatName	("fsingle.frag");
	const std::string		singleFragUintName	("usingle.frag");
	const std::string		dualFragFloatName	("fdual.frag");
	const std::string		dualFragUintName	("udual.frag");

	program.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertexName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, singleFragFloatName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, singleFragUintName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragFloatName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragUintName);

	program.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, false);

	ZShaderModule	vertex			= program.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule	singleFragFloat	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	ZShaderModule	singleFragUint	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	ZShaderModule	dualFragFloat	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 2);
	ZShaderModule	dualFragUint	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 3);

	vertex.name(vertexName);
	singleFragFloat.name(singleFragFloatName);
	singleFragUint.name(singleFragUintName);
	dualFragFloat.name(dualFragFloatName);
	dualFragUint.name(dualFragUintName);

	return
	{
		vertex,
		singleFragFloat,
		singleFragUint,
		dualFragFloat,
		dualFragUint
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

UNUSED bool isDualBlendAttachmentState (add_cref< VkPipelineColorBlendAttachmentState> s)
{
	const VkBlendFactor fs[]{
		VK_BLEND_FACTOR_SRC1_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
		VK_BLEND_FACTOR_SRC1_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA
	};

	for (const VkBlendFactor f : fs)
	{
		if (s.srcColorBlendFactor == f ||
			s.dstColorBlendFactor == f ||
			s.srcAlphaBlendFactor == f ||
			s.dstAlphaBlendFactor == f)
		{
			return true;
		}
	}

	return false;
}

ZPipeline makeBlendPipeline	(add_cref<TestParamsState>	test,
							ZPipelineLayout				layout,
							ZRenderPass					renderPass,
							add_cref<VertexInput>		vertices,
							ZShaderModule				vertex,
							ZShaderModule				singleFragmentFloat,
							ZShaderModule				dualFragmentFloat,
							ZShaderModule				singleFragmentUint,
							ZShaderModule				dualFragmentUint,
							bool						isFloatingFormat,
							bool						availableDualSourceBlend)
{
	add_cref<TestParams> p(std::get<0>(test));
	std::cout << std::get<1>(test).messagesText();

	if (p.enableDualSrcBlending && false == availableDualSourceBlend)
	{
		std::cout << "[WARNING} dualSrcBlend is not available, blend pipeline won't be created";
		return {};
	}

	VkPipelineColorBlendAttachmentState s = gpp::defaultBlendAttachmentState;
	s.blendEnable = VK_TRUE;
	s.colorBlendOp = VkBlendOp(p.colorOp);
	s.alphaBlendOp = VkBlendOp(p.alphaOp);
	s.srcColorBlendFactor = VkBlendFactor(p.srcColorFactor);
	s.dstColorBlendFactor = VkBlendFactor(p.dstColorFactor);
	s.srcAlphaBlendFactor = VkBlendFactor(p.srcAlphaFactor);
	s.dstAlphaBlendFactor = VkBlendFactor(p.dstAlphaFactor);

	ZShaderModule singleFragment = isFloatingFormat ? singleFragmentFloat : singleFragmentUint;
	ZShaderModule dualFragment = isFloatingFormat ? dualFragmentFloat : dualFragmentUint;
	ZShaderModule blendFragment = p.enableDualSrcBlending ? dualFragment : singleFragment;
	return createGraphicsPipeline(layout, renderPass, vertices, vertex, blendFragment, TestParams::defaultExtent,
									gpp::BlendAttachmentState(std::make_pair(0u, s)),
									gpp::BlendConstants(p.constColor));
}
TriLogicInt runTests (add_ref<Canvas> ctx, add_cref<std::string> assets,
						add_cref<std::vector<TestParamsState>> set, bool fromFile, bool availableDualSourceBlend)
{
	ZShaderModule				commonVert;
	ZShaderModule				singleFragFloat;
	ZShaderModule				singleFragUint;
	ZShaderModule				dualFragFloat;
	ZShaderModule				dualFragUint;
	std::tie(commonVert,
			singleFragFloat, singleFragUint,
			dualFragFloat, dualFragUint) = buildProgram(ctx.device, assets, availableDualSourceBlend);

	VertexInput					vertices	(ctx.device);
	genVertices(vertices);
	const uint32_t				vertexCount = vertices.getVertexCount(0);

	struct PushConstant
	{
		Vec4  fColor0;
		Vec4  fColor1;
		UVec4 uColor0;
		UVec4 uColor1;
	}							pc;

	LayoutManager				lm				(ctx.device);
	ZPipelineLayout				colorLayout		= lm.createPipelineLayout(ZPushRange<PushConstant>());

	ZRenderPass					colorRenderPass;
	ZImage						colorImage;
	ZImageView					colorView;
	ZFramebuffer				colorFB;
	ZPipeline					backgroundPipeline;
	ZPipeline					blendPipeline;

	EventData ev(data_count(set));
	ctx.events().cbKey.set(onKey, &ev);
	ctx.events().cbWindowSize.set(onResize, &ev);
	uint32_t lastTextIndex = INVALID_UINT32;

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>,
								ZCommandBuffer displayCmd, ZFramebuffer displayFB)
	{
		add_cref<TestParamsState>	test		(set.at(ev.testCounter));
		add_cref<TestParams>		currParams	(std::get<0>(test));
		add_cref<TestParams>		prevParams	(ev.testCounter == 0u
													? currParams
													: std::get<0>(set.at(ev.testCounter - 1u)));
		const bool			paramsChanged	(!(currParams == prevParams)); UNREF(paramsChanged);
		const bool			isFloatingFormat(true);

		if (lastTextIndex != ev.testCounter)
		{
			lastTextIndex = ev.testCounter;
			if (fromFile)
			{
				std::cout << "==============" << std::endl;
				std::cout << "Perform test: " << ev.testCounter << std::endl;
				std::cout << "Command line: " << std::get<2>(test) << std::endl;
			}
			currParams.print(std::cout, availableDualSourceBlend);
		}

		//if (ev.testCounter == 0u || currParams.colorFormat != prevParams.colorFormat || ev.refreshing)
		{
			const VkFormat colorFormat = VkFormat(currParams.colorFormat);
			colorRenderPass = createColorRenderPass(ctx.device, { colorFormat });
			const bool initialized = backgroundPipeline.has_handle()
									&& colorImage.has_handle()
									&& colorView.has_handle()
									&& colorFB.has_handle();
			UNREF(initialized);
			//if (false == initialized || ev.refreshing)
			{
				colorImage = ctx.createColorImage2D(colorFormat, TestParams::defaultExtent);
				colorView = createImageView(colorImage);
				colorFB = createFramebuffer(colorRenderPass, TestParams::defaultExtent, { colorView });
				backgroundPipeline = createGraphicsPipeline(colorLayout, colorRenderPass,
															TestParams::defaultExtent, vertices,
															commonVert, (isFloatingFormat ? singleFragFloat : singleFragUint));
				ev.refreshing = false;
			}
		}

		pc.fColor0 = currParams.color0;
		pc.fColor1 = currParams.color1;
		pc.uColor0 = currParams.color0.cast<UVec4>();
		pc.uColor1 = currParams.color1.cast<UVec4>();

		// blenPipaline may not have a handle if dualSrcBlend is not
		// supported by device and we want to use dual-source blending.
		////if ((ev.testCounter == 0u && !blendPipeline.has_handle())
		////	|| (ev.testCounter > 0u && !(currParams == prevParams)))
		{
			blendPipeline = makeBlendPipeline(test, colorLayout, colorRenderPass, vertices,
												commonVert, singleFragFloat, dualFragFloat,
				singleFragUint, dualFragUint, isFloatingFormat,
				availableDualSourceBlend);
		}

		ZImage				displayImage	= framebufferGetImage(displayFB);
		ZImageMemoryBarrier srcColorReady	(colorImage, VK_ACCESS_NONE, VK_ACCESS_NONE,
															VK_IMAGE_LAYOUT_GENERAL);
		ZImageMemoryBarrier	srcPreBlit		(colorImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstPreBlit		(displayImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(displayCmd);
			commandBufferBindVertexBuffers(displayCmd, vertices);
			commandBufferPushConstants(displayCmd, colorLayout, pc);
			{
				commandBufferBindPipeline(displayCmd, backgroundPipeline);
				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
				vkCmdDraw(*displayCmd, (vertexCount / 2u), 1, 0, 0);
				commandBufferEndRenderPass(rpbi);
			}
//			if (blendPipeline.has_handle())
//			{
//				commandBufferPipelineBarriers(displayCmd,
//											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
//											srcColorReady);
//				commandBufferBindPipeline(displayCmd, blendPipeline);
//				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
//				vkCmdDraw(*displayCmd, (vertexCount / 2u), 1, (vertexCount / 2u), 0);
//				commandBufferEndRenderPass(rpbi);
//			}
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


