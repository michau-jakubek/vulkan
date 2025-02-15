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
	bool	parsing		(std::shared_ptr<OptionInterface>, add_cref<std::string> value, add_ref<OptionParserState> state);
	auto	formatState	(add_cref<OptionT<std::string>>	sender) const -> std::string;
	auto	getState	(bool flatenize) const -> VkPipelineColorBlendAttachmentState;

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
	int						pipelineMask;
	std::string				blendingState;
	bool					noFlatenize;
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

	static inline const char* blendFactorNames[] {
		"z",     // VK_BLEND_ZERO
		"o",     // VK_BLEND_ONE
		"sc",    // VK_BLEND_SRC_COLOR
		"1msc",  // VK_BLEND_ONE_MINUS_SRC_COLOR
		"dc",    // VK_BLEND_DEST_COLOR
		"1mdc",  // VK_BLEND_ONE_MINUS_DEST_COLOR
		"sa",    // VK_BLEND_SRC_ALPHA
		"1msa",  // VK_BLEND_ONE_MINUS_SRC_ALPHA
		"da",    // VK_BLEND_DEST_ALPHA
		"1mda",  // VK_BLEND_ONE_MINUS_DEST_ALPHA
		"cc",    // VK_BLEND_CONSTANT_COLOR
		"1mcc",  // VK_BLEND_ONE_MINUS_CONSTANT_COLOR
		"ca",    // VK_BLEND_CONSTANT_ALPHA
		"1mca",  // VK_BLEND_ONE_MINUS_CONSTANT_ALPHA
		"sas",   // VK_BLEND_SRC_ALPHA_SATURATE
		"1ms1c", // VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
		"1ms1a", // VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA
		"s1c",   // VK_BLEND_FACTOR_SRC1_COLOR
		"s1a"    // VK_BLEND_FACTOR_SRC1_ALPHA
	};

	static inline const char* blendOpNames[] {
		"add",  // VK_BLEND_OP_ADD
		"sub",  // VK_BLEND_OP_SUBTRACT
		"rsub", // VK_BLEND_OP_REVERSE_SUBTRACT
		"min",  // VK_BLEND_OP_MIN
		"max",  // VK_BLEND_OP_MAX
	};


	inline static const VkExtent2D	defaultExtent		= makeExtent2D(8,8);
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
	, color0				(1, 0, 0, 1)
	, color1				(0, 1, 0, 1)
	, colorOp				(VK_BLEND_OP_ADD)
	, alphaOp				(VK_BLEND_OP_ADD)
	, srcColorFactor		(VK_BLEND_FACTOR_SRC_COLOR)
	, dstColorFactor		(VK_BLEND_FACTOR_DST_COLOR)
	, srcAlphaFactor		(VK_BLEND_FACTOR_ONE)
	, dstAlphaFactor		(VK_BLEND_FACTOR_ZERO)
	, pipelineMask			(7)
	, blendingState			()
	, noFlatenize			(false)
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
			// pipelineMask
			&& enableDualSrcBlending	== other.enableDualSrcBlending;
}

constexpr Option optionPrintBlenOps("-print-blend-ops", 0);
constexpr Option optionPrintBlendFactors("-print-blend-factors", 0);
constexpr Option optionPrintColorFormats("-print-color-formats", 0);
constexpr Option optionFile("-file", 1);
constexpr Option optionDualSource("-dual-source", 0);
constexpr Option optionNoFlatenize("-no-flatenize", 0);
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
constexpr Option optionPipelineMask("-pipeline-mask", 1, __COUNTER__);
constexpr Option optionBlendingState("-state", 1, __COUNTER__);

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

VkPipelineColorBlendAttachmentState TestParams::getState (bool flatenize) const
{
	auto flatenizeIf = [&](uint32_t factor)
	{
		VkBlendFactor flat = VK_BLEND_FACTOR_ZERO;
		if (flatenize)
			switch (VkBlendFactor(factor))
			{
			case VK_BLEND_FACTOR_SRC1_COLOR:
				flat = VK_BLEND_FACTOR_DST_COLOR;
				break;
			case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
				flat = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				break;
			case VK_BLEND_FACTOR_SRC1_ALPHA:
				flat = VK_BLEND_FACTOR_DST_ALPHA;
				break;
			case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
				flat = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			default:
				flat = VkBlendFactor(factor);
				break;
			}
		return flat;
	};

	VkPipelineColorBlendAttachmentState s{};
	s.blendEnable			= VK_TRUE;
	s.srcColorBlendFactor	= flatenizeIf(srcColorFactor);
	s.dstColorBlendFactor	= flatenizeIf(dstColorFactor);
	s.colorBlendOp			= VkBlendOp(colorOp);
	s.srcAlphaBlendFactor	= flatenizeIf(srcAlphaFactor);
	s.dstAlphaBlendFactor	= flatenizeIf(dstAlphaFactor);
	s.alphaBlendOp			= VkBlendOp(alphaOp);
	s.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	return s;
}



std::string TestParams::formatState (add_cref<OptionT<std::string>>) const
{
	std::ostringstream os;
	const VkPipelineColorBlendAttachmentState state = getState(false);
	os << "color_" << blendFactorNames[state.srcColorBlendFactor] << '_'
		<< blendFactorNames[state.dstColorBlendFactor] << '_' << blendOpNames[state.colorBlendOp];
	os << "_alpha_" << blendFactorNames[state.srcAlphaBlendFactor] << '_'
		<< blendFactorNames[state.dstAlphaBlendFactor] << '_' << blendOpNames[state.alphaBlendOp];
	if (blendingState.empty()) os << ' ' << "(*)";
	os.flush();
	return os.str();
}

bool TestParams::parsing (std::shared_ptr<OptionInterface> option, add_cref<std::string> value, add_ref<OptionParserState> state)
{
	std::string_view svalue(value);
	size_t start = 0;

	auto consume = [&](const char* s) -> bool
	{
		const std::string_view sv(s);
		if (sv == svalue.substr(start, sv.length()))
		{
			start += sv.length();
			return true;
		}
		return false;
	};

	auto consumeFactor = [&](uint32_t *factor, uint32_t err)
	{
		uint32_t i = 0u;
		for (i = 0u; i < ARRAY_LENGTH_CAST(blendFactorNames, uint32_t); ++i)
		{
			if (consume(blendFactorNames[i]))
			{
				*factor = i;
				break;
			}
		}
		if (ARRAY_LENGTH_CAST(blendFactorNames, uint32_t) == i)
			throw err;
	};

	auto consumeOp = [&](uint32_t* op, uint32_t err)
	{
		uint32_t i = 0u;
		for (i = 0u; i < ARRAY_LENGTH_CAST(blendOpNames, uint32_t); ++i)
		{
			if (consume(blendOpNames[i]))
			{
				*op = i;
				break;
			}
		}
		if (ARRAY_LENGTH_CAST(blendOpNames, uint32_t) == i)
			throw err;
	};

	auto consumeSep = [&](uint32_t err)
	{
		if (false == consume("_"))
			throw err;
	};


	if (*option == optionBlendingState)
	{
		try
		{
			uint32_t err = 0u;

			if (false == consume("color_"))
				throw ++err;

			consumeFactor(&srcColorFactor, ++err);
			consumeSep(++err);
			consumeFactor(&dstColorFactor, ++err);
			consumeSep(++err);
			consumeOp(&colorOp, ++err);

			if (false == consume("_alpha_"))
				throw ++err;

			consumeFactor(&srcAlphaFactor, ++err);
			consumeSep(++err);
			consumeFactor(&dstAlphaFactor, ++err);
			consumeSep(++err);
			consumeOp(&alphaOp, ++err);
		}
		catch (int)
		{
			state.hasErrors = true;
			state.messages << "Unable to parse " << option->getName() << " from " << std::quoted(value);
		}
		return true;
	}
	return false;
}

OptionParser<TestParams> TestParams::getParser (bool includeHelp, bool includeFile)
{
	typename OptionT<uint32_t>::format_cb formatVkBlendOp =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendOp);
	typename OptionT<uint32_t>::format_cb formatVkBlendFactor =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendFactor);
	typename OptionT<uint32_t>::format_cb formatVkFormat =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, std::map<uint32_t, std::string>());
	typename OptionT<std::string>::format_cb formatBlendingState =
		std::bind(&TestParams::formatState, this, std::placeholders::_1);

	typename OptionT<uint32_t>::parse_cb parseVkBlendOp =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
					std::placeholders::_3, std::placeholders::_4, mapVkBlendOp);
	typename OptionT<uint32_t>::parse_cb parseVkBlendFactor =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, mapVkBlendFactor);
	typename OptionT<uint32_t>::parse_cb parseVkFormat =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, std::map<uint32_t, std::string>());


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
	parser.addOption(&TestParams::noFlatenize, optionNoFlatenize,
					"Prevent fom flatenize state for non-dual-source blending",	{ params.noFlatenize }, flags);
	parser.addOption(&TestParams::constColor, optionConstColor, "Const color", { params.constColor }, flags);
	parser.addOption(&TestParams::color0, optionColor0, "Blending color 0", { params.color0 }, flags);
	parser.addOption(&TestParams::color1, optionColor1, "Blending color 1", { params.color1 }, flags);
	parser.addOption(&TestParams::pipelineMask, optionPipelineMask, "Pipeline mask", { params.pipelineMask }, flags)
		->setDesc("Enables pipelines depending on mask. 001 bit enables general pipeline. 010 enables blend pipeline."
					" 100 bit draws white square in general pipeline if dual-source is available and required.");
	parser.addOption(&TestParams::blendingState, optionBlendingState, "Blending state, do not override any factors or ops",
		{ params.blendingState }, flags, nullptr, formatBlendingState)->setDefault("\"\"");

	const std::string typeNameText = "text";
	{
		parser.addOption(&TestParams::colorOp, optionColorOp, "Color blend op",
			{ params.colorOp }, flags, parseVkBlendOp, formatVkBlendOp)
			->setDefault(TestParams::mapVkBlendOp.at(params.colorOp))->setTypeName(typeNameText);

		parser.addOption(&TestParams::alphaOp, optionAlphaOp, "Alpha blend op",
			{ params.alphaOp }, flags, parseVkBlendOp, formatVkBlendOp)
			->setDefault(TestParams::mapVkBlendOp.at(params.alphaOp))->setTypeName(typeNameText);
	}
	{
		parser.addOption(&TestParams::srcColorFactor, optionSrcColorFactor, "Source color blend factor",
			{ params.srcColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.srcColorFactor))->setTypeName(typeNameText);

		parser.addOption(&TestParams::dstColorFactor, optionDstColorFactor, "Destination color blend factor",
			{ params.dstColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.dstColorFactor))->setTypeName(typeNameText);

		parser.addOption(&TestParams::srcAlphaFactor, optionSrcAlphaFactor, "Source alpha blend factor",
			{ params.srcAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.srcAlphaFactor))->setTypeName(typeNameText);

		parser.addOption(&TestParams::dstAlphaFactor, optionDstAlphaFactor, "Destination alpha blend factor",
			{ params.dstAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.dstAlphaFactor))->setTypeName(typeNameText);
	}
	{
		parser.addOption(&TestParams::colorFormat, optionColorFormat, "Blending color format",
			{ params.dstAlphaFactor }, flags, parseVkFormat, formatVkFormat)
			->setDefault(formatGetString(defaultColorFormat))->setTypeName(typeNameText);
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
				if (false == item.empty())
					cmdLineParams.emplace_back(std::move(item));
			}

			TestParams					params	(device, assets);
			OptionParser<TestParams>	parser	(params.getParser(false, false));

			parser.parse(cmdLineParams, true, std::bind(&TestParams::parsing, &params,
														std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
		record.name, getAllocationCallbacks(), appFlags.layers, upgradeInstanceExtensions(strings()), Version(1, 3));
	ZPhysicalDevice				physicalDevice = selectPhysicalDevice(
		make_signed(appFlags.physicalDeviceIndex), instance, upgradeDeviceExtensions(strings()));

	bool							fromFile	(false);
	std::vector<TestParamsState>	set			{ std::make_tuple(TestParams(physicalDevice, record.assets),
													OptionParserState(), std::string()) };
	{
		add_ref<TestParams>			params	(std::get<0>(set.at(0)));
		add_ref<OptionParserState>	state	(std::get<1>(set.at(0)));
		OptionParser<TestParams>	parser	= params.getParser(true, true);
		parser.parse(cmdLineParams, true, std::bind(&TestParams::parsing, &params,
													std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		availableDualSourceBlend = caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::dualSrcBlend);
	};

	Canvas cs(physicalDevice, canvasStyle, onEnablingFeatures, appFlags.debugPrintfEnabled);

	return runTests(cs, record.assets, set, fromFile, availableDualSourceBlend);
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule, ZShaderModule, ZShaderModule>
buildProgram (ZDevice device, add_cref<std::string> assets, bool includeDualShaders)
{
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
	if (includeDualShaders)
	{
		program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragFloatName);
		program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragUintName);
	}

	program.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, false);

	ZShaderModule	vertex							= program.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule	dualFragFloat, singleFragFloat	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	ZShaderModule	dualFragUint, singleFragUint	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	if (includeDualShaders)
	{
		dualFragFloat	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 2);
		dualFragUint	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 3);
	}

	vertex.name(vertexName);
	singleFragFloat.name(singleFragFloatName);
	singleFragUint.name(singleFragUintName);
	if (includeDualShaders)
	{
		dualFragFloat.name(dualFragFloatName);
		dualFragUint.name(dualFragUintName);
	}

	return
	{
		vertex,
		singleFragFloat,
		singleFragUint,
		dualFragFloat,
		dualFragUint
	};
}

std::pair<ZBuffer, ZBuffer> genVertices (add_ref<VertexInput> vi)
{
	const std::vector<Vec2> vertices
	{
		{-0.75, -0.75}, {+0.50, -0.75}, {+0.50, +0.50}, {-0.75, +0.50},
		{-0.50, -0.50}, {+0.75, -0.50}, {+0.75, +0.75}, {-0.50, +0.75},
		                {-0.50, +0.50}, {+0.50, -0.50}
	};
	vi.binding(0).addAttributes(vertices);
	ZBuffer bv = vi.binding(0).getBuffer();

	const std::vector<uint32_t> indices
	{
		8,3,4, 4,3,0, 0,1,4, 4,1,9,
		4,9,2, 2,8,4,
		2,9,5, 5,6,2, 2,6,7, 7,8,2, 8,4,9, 9,2,8
	};
	ZBuffer bi = createIndexBuffer<uint32_t>(vi.device, indices);

	return { bv, bi };
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

ZPipeline makeBlendPipeline	(
	add_cref<TestParamsState>	test,
	ZPipelineLayout				layout,
	ZRenderPass					renderPass,
	add_cref<VertexInput>		vertices,
	ZShaderModule				vertex,
	ZShaderModule				singleFragmentFloat,
	ZShaderModule				singleFragmentUint,
	ZShaderModule				dualFragmentFloat,
	ZShaderModule				dualFragmentUint,
	bool						isFloatingFormat,
	bool						createDual)
{
	add_cref<TestParams> p(std::get<0>(test));
	std::cout << std::get<1>(test).messagesText();

	const VkPipelineColorBlendAttachmentState s = p.getState(false == createDual && false == p.noFlatenize);

	ZShaderModule singleFragment = isFloatingFormat ? singleFragmentFloat : singleFragmentUint;
	ZShaderModule dualFragment = isFloatingFormat ? dualFragmentFloat : dualFragmentUint;
	ZShaderModule blendFragment = (createDual && p.enableDualSrcBlending) ? dualFragment : singleFragment;
	return createGraphicsPipeline(layout, renderPass, vertices, vertex, blendFragment, TestParams::defaultExtent,
									gpp::BlendAttachmentState(std::make_pair(0u, s)),
									gpp::BlendConstants(p.constColor));
}

auto makeClearParams(ZBuffer vertexBuffer, bool isFloatingFormat, add_cref<TestParams> currParams)
	-> std::pair<VkClearAttachment, VkClearRect>
{
	const VkClearAttachment clearAttachment
	{
		VK_IMAGE_ASPECT_COLOR_BIT, 0u, {
			isFloatingFormat ? makeClearColorValue(Vec4(1))
								: makeClearColorValue(UVec4(INVALID_UINT32))
		}
	};
	std::vector<Vec2>		v(bufferGetElementCount<Vec2>(vertexBuffer));
	//std::vector<uint32_t>	i(bufferGetElementCount<uint32_t>(indexBuffer));
	bufferRead(vertexBuffer, v);
	//bufferRead(indexBuffer, i);
	UVec2 A, B, C, D;
	transformDistance(-1.0f, +1.0f, v.at(4).x(), 0u, currParams.defaultExtent.width, A.x(), false);
	transformDistance(-1.0f, +1.0f, v.at(4).y(), 0u, currParams.defaultExtent.height, A.y(), false);
	transformDistance(-1.0f, +1.0f, v.at(9).x(), 0u, currParams.defaultExtent.width, B.x(), false);
	transformDistance(-1.0f, +1.0f, v.at(9).y(), 0u, currParams.defaultExtent.height, B.y(), false);
	transformDistance(-1.0f, +1.0f, v.at(2).x(), 0u, currParams.defaultExtent.width, C.x(), false);
	transformDistance(-1.0f, +1.0f, v.at(2).y(), 0u, currParams.defaultExtent.height, C.y(), false);
	transformDistance(-1.0f, +1.0f, v.at(8).x(), 0u, currParams.defaultExtent.width, D.x(), false);
	transformDistance(-1.0f, +1.0f, v.at(8).y(), 0u, currParams.defaultExtent.height, D.y(), false);
	const VkClearRect clearRect{ {
			makeOffset2D(make_signed(A.x()), make_signed(A.y())),
			makeExtent2D(B.x() - A.x(), C.y() - B.y())
		}, 0u, 1u
	};
	return { clearAttachment, clearRect };
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

	ZBuffer						vertexBuffer;
	ZBuffer						indexBuffer;
	VertexInput					vertices	(ctx.device);
	std::tie(vertexBuffer, indexBuffer) = genVertices(vertices);

	struct PushConstant
	{
		Vec4	 fColor0;
		Vec4	 fColor1;
		UVec4	 uColor0;
		UVec4	 uColor1;
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
		const bool	useDualBlend	= availableDualSourceBlend && currParams.enableDualSrcBlending;
		const bool	paramsChanged	(!(currParams == prevParams)); UNREF(paramsChanged);
		const bool	isFloatingFormat(true);

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

		const VkFormat colorFormat = VkFormat(currParams.colorFormat);
		colorRenderPass = createColorRenderPass(ctx.device, { colorFormat });
		colorImage = ctx.createColorImage2D(colorFormat, TestParams::defaultExtent);
		colorView = createImageView(colorImage);
		colorFB = createFramebuffer(colorRenderPass, TestParams::defaultExtent, { colorView });
		backgroundPipeline = createGraphicsPipeline(colorLayout, colorRenderPass,
										TestParams::defaultExtent, vertices, commonVert,
										(isFloatingFormat ? singleFragFloat : singleFragUint));
		blendPipeline = makeBlendPipeline(test, colorLayout, colorRenderPass, vertices,
										commonVert, singleFragFloat, singleFragUint,
										dualFragFloat, dualFragUint, isFloatingFormat,
										useDualBlend);

		pc.fColor0 = currParams.color0;
		pc.fColor1 = currParams.color1;
		pc.uColor0 = currParams.color0.cast<UVec4>();
		pc.uColor1 = currParams.color1.cast<UVec4>();

		ZImage				displayImage	= framebufferGetImage(displayFB);
		ZImageMemoryBarrier	srcPreBlit		(colorImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstPreBlit		(displayImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(displayCmd);
			commandBufferBindIndexBuffer(displayCmd, indexBuffer);
			commandBufferBindVertexBuffers(displayCmd, vertices);
			if (currParams.pipelineMask & 0b001)
			{
				commandBufferPushConstants(displayCmd, colorLayout, pc);
				commandBufferBindPipeline(displayCmd, backgroundPipeline);
				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
				vkCmdDrawIndexed(*displayCmd, 12, 1, 0, 0, 0);
				vkCmdDrawIndexed(*displayCmd, 12, 1, 18, 0, 1);
				if (useDualBlend)
				{
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 2);
				}
				else
				{
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 0);
				}
				commandBufferEndRenderPass(rpbi);
			}
			if (currParams.pipelineMask & 0b010)
			{
				ZImageMemoryBarrier srcColorReady(colorImage, VK_ACCESS_NONE, VK_ACCESS_NONE,
					VK_IMAGE_LAYOUT_GENERAL);

				commandBufferPipelineBarriers(displayCmd,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
											srcColorReady);
				commandBufferPushConstants(displayCmd, colorLayout, pc);
				commandBufferBindPipeline(displayCmd, blendPipeline);
				auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
				if (useDualBlend)
				{
					VkClearRect clearRect;
					VkClearAttachment clearAttachment;
					std::tie(clearAttachment, clearRect) = makeClearParams(vertexBuffer, isFloatingFormat, currParams);
					vkCmdClearAttachments(*displayCmd, 1u, &clearAttachment, 1u, &clearRect);
				}
				vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 1);
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


