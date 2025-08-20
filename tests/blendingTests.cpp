#include "blendingTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfOptionParser.hpp"
#include "vtfTermColor.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfShaderObjectCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfFloat16.hpp"

#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

namespace
{

using namespace vtf;
using ostream_ref = add_ref<std::ostream>;

struct OptionParserStateX : OptionParserState
{
	bool hasSrc0, hasSrc1, hasSrcTwice;
	OptionParserStateX() : hasSrc0(false), hasSrc1(false), hasSrcTwice(false) {}
};

struct TestParams
{
	typedef std::map<uint32_t, std::pair<std::string, std::string>> Map;
	typedef std::pair<const uint32_t, std::pair<std::string, std::string>> MapItem;

			TestParams	(ZPhysicalDevice physicalDevice, add_cref<std::string> assets_);
	auto	getParser	(bool includeHelp, bool includeFile, bool includeColorTwice) -> OptionParser<TestParams, OptionParserStateX>;
	void	print		(ostream_ref log, bool availableDualSourceBlend, add_cref<OptionParserStateX>) const;
	void	printOps	(ostream_ref log) const;
	void	printCpp	(ostream_ref log) const;
	void	printFactors(ostream_ref log) const;
	void	printFormats(ostream_ref log) const;
	auto	parseEnum	(add_cref<std::string>			text,
						add_cref<OptionT<uint32_t>>		sender,
						add_ref<bool>					status,
						add_ref<OptionParserState>		state,
						add_cref<Map>					map) const -> uint32_t;
	auto	formatEnum	(add_cref<OptionT<uint32_t>>	sender,
						add_cref<Map>					map) const -> std::string;
	bool	operator==	(add_cref<TestParams> other) const;
	bool	parsing		(std::shared_ptr<OptionInterface>, add_cref<std::string> value, add_ptr<OptionParserState> state);
	auto	formatState	(add_cref<OptionT<std::string>>	sender, bool owner) const -> std::string;
	auto	getState	(add_ptr<VkBool32> flatenize = nullptr) const -> VkPipelineColorBlendAttachmentState;

	ZPhysicalDevice			device;
	add_cref<std::string>	assets;
	std::string				file;
	VkExtent2D				extent;
	uint32_t				colorFormat;
	Vec4					constColor;
	Vec4					srcColor0;
	Vec4					dstColor;
	Vec4					srcTwiceColor;
	Vec4					srcColor1;
	uint32_t				colorOp;
	uint32_t				alphaOp;
	uint32_t				srcColorFactor;
	uint32_t				dstColorFactor;
	uint32_t				srcAlphaFactor;
	uint32_t				dstAlphaFactor;
	uint32_t				colorWriteMask;
	float					epsilon;
	std::string				blendingState;
	bool					enableDualSrcBlending;
	bool					enableShaderObject;
	bool					printBlendOps;
	bool					printBlendFactors;
	bool					printColorFormats;
	bool					printCppCode;

	static inline const Map mapVkBlendOp
	{
		{ VK_BLEND_OP_ADD,				{ "VK_BLEND_OP_ADD",				"add"  }},
		{ VK_BLEND_OP_SUBTRACT,			{ "VK_BLEND_OP_SUBTRACT",			"sub"  }},
		{ VK_BLEND_OP_REVERSE_SUBTRACT,	{ "VK_BLEND_OP_REVERSE_SUBTRACT",	"rsub" }},
		{ VK_BLEND_OP_MIN,				{ "VK_BLEND_OP_MIN",				"min"  }},
		{ VK_BLEND_OP_MAX,				{ "VK_BLEND_OP_MAX",				"max"  }},
	};

	static inline const Map mapVkBlendFactor
	{
		{ VK_BLEND_FACTOR_ZERO						, { "VK_BLEND_FACTOR_ZERO", "z" }},
		{ VK_BLEND_FACTOR_ONE						, { "VK_BLEND_FACTOR_ONE", "o" }},
		{ VK_BLEND_FACTOR_SRC_COLOR					, { "VK_BLEND_FACTOR_SRC_COLOR", "sc" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR		, { "VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR", "1msc" }},
		{ VK_BLEND_FACTOR_DST_COLOR					, { "VK_BLEND_FACTOR_DST_COLOR", "dc" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR		, { "VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR", "1mdc" }},
		{ VK_BLEND_FACTOR_SRC_ALPHA					, { "VK_BLEND_FACTOR_SRC_ALPHA", "sa" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA		, { "VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA", "1msa" }},
		{ VK_BLEND_FACTOR_DST_ALPHA					, { "VK_BLEND_FACTOR_DST_ALPHA", "da" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA		, { "VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA", "1mda" }},
		{ VK_BLEND_FACTOR_CONSTANT_COLOR			, { "VK_BLEND_FACTOR_CONSTANT_COLOR", "cc" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR	, { "VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR", "1mcc" }},
		{ VK_BLEND_FACTOR_CONSTANT_ALPHA			, { "VK_BLEND_FACTOR_CONSTANT_ALPHA", "ca" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA	, { "VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA", "1mca" }},
		{ VK_BLEND_FACTOR_SRC_ALPHA_SATURATE		, { "VK_BLEND_FACTOR_SRC_ALPHA_SATURATE", "sas" }},
		{ VK_BLEND_FACTOR_SRC1_COLOR				, { "VK_BLEND_FACTOR_SRC1_COLOR", "s1c" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR		, { "VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR", "1ms1c" }},
		{ VK_BLEND_FACTOR_SRC1_ALPHA				, { "VK_BLEND_FACTOR_SRC1_ALPHA", "s1a" }},
		{ VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA		, { "VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA", "1ms1a" }},
	};

	inline static const VkExtent2D	defaultExtent		= makeExtent2D(32,32);
	inline static const VkFormat	defaultColorFormat	= VK_FORMAT_R32G32B32A32_SFLOAT;
};
typedef std::tuple<TestParams, OptionParserStateX, std::string> TestParamsState;

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
	, srcColor0				(0, 1, 0, 0.875)
	, dstColor				(1, 0, 0, 0.125)
	, srcTwiceColor			(srcColor0)
	, srcColor1				(srcColor0)
	, colorOp				(VK_BLEND_OP_ADD)
	, alphaOp				(VK_BLEND_OP_ADD)
	, srcColorFactor		(VK_BLEND_FACTOR_SRC_COLOR)
	, dstColorFactor		(VK_BLEND_FACTOR_DST_COLOR)
	, srcAlphaFactor		(VK_BLEND_FACTOR_ONE)
	, dstAlphaFactor		(VK_BLEND_FACTOR_ONE)
	, colorWriteMask		(15)
	, epsilon				(0.001f)
	, blendingState			()
	, enableDualSrcBlending	(false)
	, enableShaderObject	(false)
	, printBlendOps			(false)
	, printBlendFactors		(false)
	, printColorFormats		(false)
	, printCppCode			(false)
{
}
void TestParams::print (add_ref<std::ostream> log, bool availableDualSourceBlend, add_cref<OptionParserStateX> state) const
{
	typedef routine_arg_t<decltype(std::setw), 0> sted_setw_0;
	TestParams	params	(*this);
	auto		parser	= params.getParser(true, true, state.hasSrcTwice);
	const sted_setw_0 w = static_cast<sted_setw_0>(std::max(24u, parser.getMaxOptionNameLength()));

	log << std::left << std::setw(w) << "Extent" << UVec2(extent.width, extent.height) << std::endl;
	log << std::left << std::setw(w) << "dualSrcBlend" << boolean(availableDualSourceBlend) << std::endl;

	for (auto opt : parser.getOptions())
	{
		log << std::left << std::setw(w) << opt->getName() << opt->valueWriter() << std::endl;
	}

	if (printCppCode)
	{
		printCpp(log);
	}
}
void TestParams::printOps (ostream_ref log) const
{
	log << "  VkBlendOp  \n-------------\n";
	for (add_cref<MapItem> item : mapVkBlendOp)
	{
		log << "  " << item.second.first << std::endl;
	}
}
void TestParams::printFactors (ostream_ref log) const
{
	log << "  VkBlendFactor  \n-----------------\n";
	for (add_cref<MapItem> item : mapVkBlendFactor)
	{
		log << "  " << item.second.first << std::endl;
	}
}
void TestParams::printFormats (ostream_ref log) const
{
	log << "  VkFormat  \n------------\n";
	uint32_t j = 0u;
	std::string_view::size_type maxNameLen = 0;
	ZFormatInfoIterator i;
	while (i.next())
	{
		if (doesFormatSupportBlending(device, i.format))
			maxNameLen = std::max(maxNameLen, std::string_view(i.name).length());
	}
	i.reset();
	while (i.next())
	{
		if (doesFormatSupportBlending(device, i.format))
			log << "  " << std::setw(3) << (j++) << ": " << std::setw(0)
				<< std::left << std::setw(routine_arg_t<decltype(std::setw), 0>(maxNameLen)) << i.name << std::setw(0)
				<< " float: " << boolean(i.floating)
				<< ", signed: " << boolean(i.isSigned)
				<< std::endl;
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
						add_cref<std::vector<TestParamsState>> set, bool fromFile,
						bool availableDualSourceBlend, bool needShaderObject);

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
			&& colorWriteMask			== other.colorWriteMask
			&& enableDualSrcBlending	== other.enableDualSrcBlending
			&& enableShaderObject		== other.enableShaderObject;
}

constexpr Option optionPrintBlenOps("-print-blend-ops", 0);
constexpr Option optionPrintBlendFactors("-print-blend-factors", 0);
constexpr Option optionPrintColorFormats("-print-color-formats", 0);
constexpr Option optionPrintCppCode("-print-cpp-code", 0);
constexpr Option optionFile("-file", 1);
constexpr Option optionDualSource("-dual-source", 0);
constexpr Option optionShaderObject("-shader-object", 0);
constexpr Option optionSrcColor1("-src1", 1);
constexpr Option optionConstColor("-const-color", 1);
constexpr Option optionSrcColor0("-src", 1, __COUNTER__);
constexpr Option optionSrcColorTwice("-src-twice", 1, __COUNTER__);
constexpr Option optionDstColor("-dst", 1, __COUNTER__);
constexpr Option optionColorFormat("-format", 1, __COUNTER__);
constexpr Option optionColorOp("-color-op", 1, __COUNTER__);
constexpr Option optionAlphaOp("-alpha-op", 1, __COUNTER__);
constexpr Option optionSrcColorFactor("-src-color-factor", 1, __COUNTER__);
constexpr Option optionDstColorFactor("-dst-color-factor", 1, __COUNTER__);
constexpr Option optionSrcAlphaFactor("-src-alpha-factor", 1, __COUNTER__);
constexpr Option optionDstAlphaFactor("-dst-alpha-factor", 1, __COUNTER__);
constexpr Option optionColorWriteMask("-color-write-mask", 1, __COUNTER__);
constexpr Option optionBlendingState("-state", 1, __COUNTER__);
constexpr Option optionEpsilon("-epsilon", 1, __COUNTER__);

void TestParams::printCpp(ostream_ref log) const
{
	TestParams params(*this);
	auto parser = params.getParser(false, false, false);
	auto opt = parser.getOptionByName(optionPrintCppCode);
	auto optt = std::dynamic_pointer_cast<OptionT<std::string>>(opt);

	static const std::pair<VkColorComponentFlagBits, add_cptr<char>> bits[]{
		{ VK_COLOR_COMPONENT_R_BIT, STRINGIZE(VK_COLOR_COMPONENT_R_BIT) },
		{ VK_COLOR_COMPONENT_G_BIT,	STRINGIZE(VK_COLOR_COMPONENT_G_BIT) },
		{ VK_COLOR_COMPONENT_B_BIT,	STRINGIZE(VK_COLOR_COMPONENT_B_BIT) },
		{ VK_COLOR_COMPONENT_A_BIT,	STRINGIZE(VK_COLOR_COMPONENT_A_BIT) },
	};

	log << colorize("/********* CPP CODE BEGIN ************/\n", TermColor::YELLOW)
		<< "const VkFormat format = " << formatGetInfo(VkFormat(colorFormat)).name << ";\n"
		<< "const Vec4 dstColor(" << dstColor.r() << ", " << dstColor.g() << ", "
								  << dstColor.b() << ", " << dstColor.a() << ");\n"
		<< "const Vec4 color0(" << srcColor0.r() << ", " << srcColor0.g() << ", "
								<< srcColor0.b() << ", " << srcColor0.a() << ");\n"
		<< "const Vec4 color1(" << srcColor1.r() << ", " << srcColor1.g() << ", "
								<< srcColor1.b() << ", " << srcColor1.a() << ");\n"
		<< "const Vec4 constColor(" << constColor.r() << ", " << constColor.g() << ", "
								<< constColor.b() << ", " << constColor.a() << ");\n"
		<< "const char* state = \"" << formatState(*optt, false) << "\";\n"
		<< "VkPipelineColorBlendAttachmentState s{};\n"
		<< "s.blendEnable = VK_TRUE;\n"
		<< "s.srcColorBlendFactor = " << mapVkBlendFactor.at(srcColorFactor).first << ";\n"
		<< "s.dstColorBlendFactor = " << mapVkBlendFactor.at(dstColorFactor).first << ";\n"
		<< "s.colorBlendOp = " << mapVkBlendOp.at(colorOp).first << ";\n"
		<< "s.srcAlphaBlendFactor = " << mapVkBlendFactor.at(srcAlphaFactor).first << ";\n"
		<< "s.dstAlphaBlendFactor = " << mapVkBlendFactor.at(dstAlphaFactor).first << ";\n"
		<< "s.alphaBlendOp = " << mapVkBlendOp.at(alphaOp).first << ";\n"
		<< "s.colorWriteMask = ";

	bool any = false;
	for (add_cref<std::pair<VkColorComponentFlagBits, add_cptr<char>>> bit : bits)
	{
		if (bit.first & colorWriteMask)
		{
			if (any) log << " | ";
			log << bit.second;
			any = true;
		}
	}
	if (any)
		log << ";\n";
	else log << "VkColorComponentFlags(0);\n";

	log << colorize("/********** CPP CODE END *************/\n", TermColor::YELLOW);
}

std::string TestParams::formatEnum (add_cref<OptionT<uint32_t>> sender,	add_cref<Map> map) const
{
	if (sender.id == optionColorFormat.id)
	{
		add_cptr<char> s = formatGetString(VkFormat(sender.m_storage));
		return s ? s : "VK_FORMAT_UNDEFINED";
	}
	return map.at(sender.m_storage).first;
}

uint32_t TestParams::parseEnum	(
	add_cref<std::string>		text,
	add_cref<OptionT<uint32_t>>	sender,
	add_ref<bool>				status,
	add_ref<OptionParserState>	state,
	add_cref<Map>				map) const
{
	uint32_t mc = 0u;
	uint32_t bm = INVALID_UINT32;
	const std::string upperText = toUpper(text);
	const std::string_view svText(upperText);

	Map formatMap;
	if (sender.id == optionColorFormat.id)
	{
		ZFormatInfoIterator	it;
		while (it.next())
		{
			if (doesFormatSupportBlending(device, it.format))
				formatMap[uint32_t(it.format)] = { it.name, "" };
		}
	}

	for (add_cref<MapItem> item : (sender.id == optionColorFormat.id) ? formatMap : map)
	{
		const std::string_view svItem(item.second.first);
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

VkPipelineColorBlendAttachmentState TestParams::getState (add_ptr<VkBool32> flatenize) const
{
	if (flatenize)
	{
		*flatenize = VK_FALSE;
	}

	auto flatenizeIf = [&](uint32_t factor)
	{
		VkBlendFactor f = VkBlendFactor(factor);
		if (flatenize)
		{
			switch (factor)
			{
			case VK_BLEND_FACTOR_SRC1_COLOR:
				f = VK_BLEND_FACTOR_SRC_COLOR;
				*flatenize |= VK_TRUE;
				break;
			case VK_BLEND_FACTOR_SRC1_ALPHA:
				f = VK_BLEND_FACTOR_SRC1_ALPHA;
				*flatenize |= VK_TRUE;
				break;
			case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
				f = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				*flatenize |= VK_TRUE;
				break;
			case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
				f = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
				*flatenize |= VK_TRUE;
				break;
			default: break;
			}
		}
		return f;
	};

	VkPipelineColorBlendAttachmentState s{};
	s.blendEnable			= VK_TRUE;
	s.srcColorBlendFactor	= flatenizeIf(srcColorFactor);
	s.dstColorBlendFactor	= flatenizeIf(dstColorFactor);
	s.colorBlendOp			= VkBlendOp(colorOp);
	s.srcAlphaBlendFactor	= flatenizeIf(srcAlphaFactor);
	s.dstAlphaBlendFactor	= flatenizeIf(dstAlphaFactor);
	s.alphaBlendOp			= VkBlendOp(alphaOp);
	s.colorWriteMask		= VkColorComponentFlags(colorWriteMask);
	return s;
}

std::string TestParams::formatState (add_cref<OptionT<std::string>>, bool owner) const
{
	std::ostringstream os;
	const VkPipelineColorBlendAttachmentState blendState = getState();

	auto name = [&](auto field) -> std::string
	{
		if constexpr (std::is_same_v<decltype(field), VkBlendFactor>)
		{
			if (auto k = mapVkBlendFactor.find(uint32_t(field)); k != mapVkBlendFactor.end())
				return k->second.second;
		}
		else
		{
			if (auto k = mapVkBlendOp.find(uint32_t(field)); k != mapVkBlendOp.end())
				return k->second.second;
		}
		return "???";
	};

	os	<< "color_"		<< name(blendState.srcColorBlendFactor)
		<< "_"			<< name(blendState.dstColorBlendFactor)
		<< "_"			<< name(blendState.colorBlendOp)
		<< "_alpha_"	<< name(blendState.srcAlphaBlendFactor)
		<< "_"			<< name(blendState.dstAlphaBlendFactor)
		<< "_"			<< name(blendState.alphaBlendOp);

	if (owner && blendingState.empty()) os << ' ' << "(*)";

	os.flush();
	return os.str();
}

bool TestParams::parsing (
	std::shared_ptr<OptionInterface>	option,
	add_cref<std::string>				value,
	add_ptr<OptionParserState>			state)
{
	auto pState = static_cast<add_ptr<OptionParserStateX>>(state);

	enum StateErr
	{
		Separator,
		ColorPrefix,
		SrcColorFactor,
		DstColorFactor,
		ColorOp,
		AlphaPrefix,
		SrcAlphaFactor,
		DstAlphaFactor,
		AlphaOp
	};

	std::string_view svalue(value);
	size_t start = 0;

	auto consume = [&](add_cref<std::string> s) -> bool
	{
		const std::string_view sv(s);
		if (sv == svalue.substr(start, sv.length()))
		{
			start += sv.length();
			return true;
		}
		return false;
	};

	auto consumeFactor = [&](uint32_t *factor, StateErr err)
	{
		bool found = false;
		for (add_cref<std::pair<const uint32_t, std::pair<std::string, std::string>>> item : mapVkBlendFactor)
		{
			if (consume(item.second.second))
			{
				*factor = item.first;
				found = true;
				break;
			}
		}
		if (false == found) throw err;
	};

	auto consumeOp = [&](uint32_t* op, StateErr err)
	{
		bool found = false;
		for (add_cref<std::pair<const uint32_t, std::pair<std::string, std::string>>> item : mapVkBlendOp)
		{
			if (consume(item.second.second))
			{
				*op = item.first;
				found = true;
				break;
			}
		}
		if (false == found) throw err;
	};

	auto consumeSep = [&](StateErr err)
	{
		if (false == consume("_"))
			throw err;
	};

	auto colorsConflict = [&]()
	{
		pState->hasErrors = true;
		pState->messages << "If params contains " << std::quoted(optionSrcColor0.name)
						 << " and/or " << std::quoted(optionSrcColor1.name)
						 << " the " << std::quoted(optionSrcColorTwice.name) << " must not be specified." << std::endl;
	};

	if (*option == optionSrcColor0)
	{
		pState->hasSrc0 = true;
		if (pState->hasSrcTwice)
			colorsConflict();
	}
	else if (*option == optionSrcColor1)
	{
		pState->hasSrc1 = true;
		if (pState->hasSrcTwice)
			colorsConflict();
	}
	else if (*option == optionSrcColorTwice)
	{
		pState->hasSrcTwice = true;
		if (pState->hasSrc0 || pState->hasSrc1)
			colorsConflict();
	}
	else if (*option == optionBlendingState)
	{
		try
		{
			if (false == consume("color_"))
				throw ColorPrefix;

			consumeFactor(&srcColorFactor, SrcColorFactor);
			consumeSep(Separator);
			consumeFactor(&dstColorFactor, DstColorFactor);
			consumeSep(Separator);
			consumeOp(&colorOp, ColorOp);

			if (false == consume("_alpha_"))
				throw AlphaPrefix;

			consumeFactor(&srcAlphaFactor, SrcAlphaFactor);
			consumeSep(Separator);
			consumeFactor(&dstAlphaFactor, DstAlphaFactor);
			consumeSep(Separator);
			consumeOp(&alphaOp, AlphaOp);
		}
		catch (StateErr err)
		{
			const char* what = nullptr;
			switch (err)
			{
			case Separator:			what = STRINGIZE(Separator);		break;
			case ColorPrefix:		what = STRINGIZE(ColorPrefix);		break;
			case SrcColorFactor:	what = STRINGIZE(SrcColorFactor);	break;
			case DstColorFactor:	what = STRINGIZE(DstColorFactor);	break;
			case ColorOp:			what = STRINGIZE(ColorOp);			break;
			case AlphaPrefix:		what = STRINGIZE(AlphaPrefix);		break;
			case SrcAlphaFactor:	what = STRINGIZE(SrcAlphaFactor);	break;
			case DstAlphaFactor:	what = STRINGIZE(DstAlphaFactor);	break;
			case AlphaOp:			what = STRINGIZE(AlphaOp);			break;
			}

			pState->hasErrors = true;
			pState->messages << "Unable to parse " << option->getName()
							<< " from " << std::quoted(value) << ", invalid " << what;
		}

		blendingState = value;
		return true;
	}

	return false;
}

OptionParser<TestParams, OptionParserStateX> TestParams::getParser (bool includeHelp, bool includeFile, bool includeColorTwice)
{
	typename OptionT<uint32_t>::format_cb formatVkBlendOp =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendOp);
	typename OptionT<uint32_t>::format_cb formatVkBlendFactor =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, mapVkBlendFactor);
	typename OptionT<uint32_t>::format_cb formatVkFormat =
		std::bind(&TestParams::formatEnum, this, std::placeholders::_1, Map());
	typename OptionT<std::string>::format_cb formatBlendingState =
		std::bind(&TestParams::formatState, this, std::placeholders::_1, true);

	typename OptionT<uint32_t>::parse_cb parseVkBlendOp =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
					std::placeholders::_3, std::placeholders::_4, mapVkBlendOp);
	typename OptionT<uint32_t>::parse_cb parseVkBlendFactor =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, mapVkBlendFactor);
	typename OptionT<uint32_t>::parse_cb parseVkFormat =
		std::bind(&TestParams::parseEnum, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4, Map());


	OptionFlags										flags	(OptionFlag::PrintDefault);
	add_ref<TestParams>								params	= *this;
	OptionParser<TestParams, OptionParserStateX>	parser	(params, includeHelp);

	if (includeHelp)
	{
		parser.addOption(&TestParams::printBlendOps, optionPrintBlenOps,
			"Print available VkBlendOp enum values", { params.printBlendOps }, flags);
		parser.addOption(&TestParams::printBlendFactors, optionPrintBlendFactors,
			"Print available VkBlendFactor enum values", { params.printBlendOps }, flags);
		parser.addOption(&TestParams::printColorFormats, optionPrintColorFormats,
			"Print available VkFormat enum values", { params.printColorFormats }, flags);
		parser.addOption(&TestParams::printCppCode, optionPrintCppCode,
			"Print blending params as CPP code", { params.printCppCode }, flags);
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
		"Enable dual source blending. Another shader with two outputs will be used.",
		{ params.enableDualSrcBlending }, flags);
	parser.addOption(&TestParams::enableShaderObject, optionShaderObject,
		"Run test with dynamic rendering and shader objects instead of regular pipeline",
		{ params.enableShaderObject }, flags);
	parser.addOption(&TestParams::constColor, optionConstColor, "Const color", { params.constColor }, flags);
	parser.addOption(&TestParams::srcColor0, optionSrcColor0, "Source color 0", { params.srcColor0 }, flags);
	parser.addOption(&TestParams::srcColor1, optionSrcColor1, "Source color 1", { params.srcColor1 }, flags);
	if (includeColorTwice)
	{
		parser.addOption(&TestParams::srcTwiceColor, optionSrcColorTwice, "Concurrently sets scr and src1", { params.srcTwiceColor }, flags);
	}
	parser.addOption(&TestParams::dstColor, optionDstColor, "Desstination color", { params.dstColor }, flags);
	parser.addOption(&TestParams::colorWriteMask, optionColorWriteMask, "Color write mask", { params.colorWriteMask }, flags);
	parser.addOption(&TestParams::epsilon, optionEpsilon, "Epsilon for component comparing", { params.epsilon }, flags);
	parser.addOption(&TestParams::blendingState, optionBlendingState, "Blending state, do not override any factors or ops",
		{ params.blendingState }, flags, nullptr, formatBlendingState)->setDefault("\"\"");

	const std::string typeNameText = "text";
	{
		parser.addOption(&TestParams::srcColorFactor, optionSrcColorFactor, "Source color blend factor",
			{ params.srcColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.srcColorFactor).first)->setTypeName(typeNameText);

		parser.addOption(&TestParams::dstColorFactor, optionDstColorFactor, "Destination color blend factor",
			{ params.dstColorFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.dstColorFactor).first)->setTypeName(typeNameText);

		parser.addOption(&TestParams::colorOp, optionColorOp, "Color blend op",
			{ params.colorOp }, flags, parseVkBlendOp, formatVkBlendOp)
			->setDefault(TestParams::mapVkBlendOp.at(params.colorOp).first)->setTypeName(typeNameText);

		parser.addOption(&TestParams::srcAlphaFactor, optionSrcAlphaFactor, "Source alpha blend factor",
			{ params.srcAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.srcAlphaFactor).first)->setTypeName(typeNameText);

		parser.addOption(&TestParams::dstAlphaFactor, optionDstAlphaFactor, "Destination alpha blend factor",
			{ params.dstAlphaFactor }, flags, parseVkBlendFactor, formatVkBlendFactor)
			->setDefault(TestParams::mapVkBlendFactor.at(params.dstAlphaFactor).first)->setTypeName(typeNameText);

		parser.addOption(&TestParams::alphaOp, optionAlphaOp, "Alpha blend op",
			{ params.alphaOp }, flags, parseVkBlendOp, formatVkBlendOp)
			->setDefault(TestParams::mapVkBlendOp.at(params.alphaOp).first)->setTypeName(typeNameText);
	}
	{
		parser.addOption(&TestParams::colorFormat, optionColorFormat, "Blending color format",
			{ params.colorFormat }, flags, parseVkFormat, formatVkFormat)
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
			OptionParser<TestParams,
				OptionParserStateX>		parser	(params.getParser(false, false, true));

			parser.parse(cmdLineParams, true, std::bind(&TestParams::parsing, &params,
														std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
			OptionParserStateX			state	= parser.getState();

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
													OptionParserStateX(), std::string()) };
	{
		add_ref<TestParams>			params	(std::get<0>(set.at(0)));
		add_ref<OptionParserStateX>	state	(std::get<1>(set.at(0)));
		auto						parser	= params.getParser(true, true, true);
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

		if (state.hasSrcTwice)
		{
			params.srcColor0 = params.srcTwiceColor;
			params.srcColor1 = params.srcTwiceColor;
		}

		auto optFile = parser.getOptionByName(optionFile);
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

	bool needShaderObject = false;
	for (add_cref<TestParamsState> state : set)
	{
		if (std::get<TestParams>(state).enableShaderObject)
		{
			needShaderObject = true;
			break;
		}
	}

	bool availableDualSourceBlend = false;
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		availableDualSourceBlend = caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::dualSrcBlend);

		if (needShaderObject)
		{
			caps.addUpdateFeatureIf(&VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject)
				.checkSupported("shaderObject");
			caps.addExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

			caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
				.checkSupported("dynamicRendering");
			caps.addExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

			caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState)
				.checkSupported("extendedDynamicState");
			caps.addExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

			/*
			caps.addUpdateFeatureIf(&VkPhysicalDeviceColorWriteEnableFeaturesEXT::colorWriteEnable)
				.checkSupported("colorWriteEnable");
			caps.addExtension(VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME);
			*/
		}
	};

	Canvas cs(physicalDevice, canvasStyle, onEnablingFeatures, appFlags.debugPrintfEnabled);

	return runTests(cs, record.assets, set, fromFile, availableDualSourceBlend, needShaderObject);
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule>
buildProgram (ZDevice device, add_cref<std::string> assets, bool includeDualShaders)
{
	const GlobalAppFlags	flags			(getGlobalAppFlags());
	ProgramCollection		program			(device, assets);

	const std::string		vertexName			("common.vert");
	const std::string		genericFragName		("generic.frag");
	const std::string		dualFragName		("dual.frag");

	program.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertexName);
	program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, genericFragName);
	if (includeDualShaders)
	{
		program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragName);
	}

	program.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, false);

	ZShaderModule	vertex					= program.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule	dualFrag, genericFrag	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	if (includeDualShaders)
	{
		dualFrag	= program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	}

	vertex.name(vertexName);
	genericFrag.name(genericFragName);
	if (includeDualShaders)
	{
		dualFrag.name(dualFragName);
	}

	return
	{
		vertex,
		genericFrag,
		dualFrag
	};
}

template<class PC_>
std::tuple<ZShaderObject, ZShaderObject, ZShaderObject>
buildProgram(ZDevice device, add_cref<std::string> assets, bool includeDualShaders)
{
	const GlobalAppFlags	flags(getGlobalAppFlags());
	ShaderObjectCollection	program(device, assets);

	const std::string		vertexName("common.vert");
	const std::string		genericFragName("generic.frag");
	const std::string		dualFragName("dual.frag");

	const ZPushRange<PC_> range(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	using Link = ShaderObjectCollection::ShaderLink;
	Link vertexLink				= program.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertexName);
	Link dualLink, genericLink	= program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, genericFragName);

	/* 
	 * ==================== WHY? =======================
	 * VUID - vkCmdDrawIndexed - None - 08878(ERROR / SPEC)
	 * type = VK_OBJECT_TYPE_COMMAND_BUFFER; | MessageID = 0xb422117d
	 * Shaders VK_SHADER_STAGE_VERTEX_BIT and VK_SHADER_STAGE_FRAGMENT_BIT have different push constant ranges.
	 * The Vulkan spec states : All bound graphics shader objects must have been created with identical or
	 * identically defined push constant ranges
	 * (https ://vulkan.lunarg.com/doc/view/1.4.304.1/windows/antora/spec/latest/chapters/drawing.html#VUID-vkCmdDrawIndexed-None-08878)
	 */
	if (includeDualShaders)
	{
		dualLink = program.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, dualFragName);
		program.updateShaders({ vertexLink, genericLink, dualLink }, false, flags.spirvValidate, flags.genSpirvDisassembly);
		program.updateShaders({ vertexLink, genericLink, dualLink }, range);
	}
	else
	{
		program.updateShaders({ vertexLink, genericLink }, false, flags.spirvValidate, flags.genSpirvDisassembly);
		program.updateShaders({ vertexLink, genericLink }, range);
	}

	program.buildAndVerify();

	ZShaderObject	vertex = program.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderObject	dualFrag, genericFrag = program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	if (includeDualShaders)
	{
		dualFrag = program.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	}

	vertex.name(vertexName);
	genericFrag.name(genericFragName);
	if (includeDualShaders)
	{
		dualFrag.name(dualFragName);
	}

	return
	{
		vertex,
		genericFrag,
		dualFrag
	};
}

std::pair<ZBuffer, ZBuffer> genVertices (add_ref<VertexInput> vi)
{
	const std::vector<Vec2> vertices
	{
		{-0.75, -0.75}, {+0.50, -0.75}, {+0.50, +0.50}, {-0.75, +0.50},
		{-0.50, -0.50}, {+0.75, -0.50}, {+0.75, +0.75}, {-0.50, +0.75},
		                {-0.50, +0.50}, {+0.50, -0.50},
		                {+0.50, +0.75}, {+0.75, +0.50}
	};
	vi.binding(0).addAttributes(vertices);
	ZBuffer bv = vi.binding(0).getBuffer();

	const std::vector<uint32_t> indices
	{
		8,3,4, 4,3,0, 0,1,4, 4,1,9,	// 0-12
		4,9,2, 2,8,4,				// 12-6
		2,9,5, 5,6,2, 2,6,7, 7,8,2, // 18-12
		10,2,11, 11,6,10			// 30-6
	};
	ZBuffer bi = createIndexBuffer<uint32_t>(vi.device, indices);

	return { bv, bi };
}

[[maybe_unused]] bool isDualBlendAttachmentState (add_cref<VkPipelineColorBlendAttachmentState> s)
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

/*
void printFlatnizeWarning (add_ref<std::ostream> log, add_cref<TestParams> params, bool availableDualSourceBlend)
{
	if (isDualBlendAttachmentState(params.getState())
		&& !(availableDualSourceBlend && params.enableDualSrcBlending))
	{
		std::ostringstream str;
		str << "WARNING: " << std::quoted(optionDualSource.name) << " is not enabled "
			<< "or dual-source blending is not available on this device, "
			<< "generic factors will be applied";
		log << colorize(str.str(), TermColor::DYELLOW) << std::endl;
	}
}
*/

ZPipeline makeBlendPipeline	(
	add_cref<TestParamsState>	test,
	ZPipelineLayout				layout,
	ZRenderPass					renderPass,
	add_cref<VertexInput>		vertices,
	ZShaderModule				vertex,
	ZShaderModule				genericFragment,
	ZShaderModule				dualFragment,
	bool						availableDualSourceBlend)
{
	add_cref<TestParams> p(std::get<0>(test));
	std::cout << std::get<1>(test).messagesText();

	const bool useDualSource = p.enableDualSrcBlending && availableDualSourceBlend;
	const VkPipelineColorBlendAttachmentState s = p.getState();
	ZShaderModule blendFragment = useDualSource ? dualFragment : genericFragment;
	return createGraphicsPipeline(layout, renderPass, vertices, vertex, blendFragment, TestParams::defaultExtent,
									gpp::BlendAttachmentState(std::make_pair(0u, s)),
									gpp::BlendConstants(p.constColor));
}

void readColors (
	ZBuffer					vertexBuffer,
	ZBuffer					colorBuffer,
	add_cref<VkExtent2D>	extent,
	add_cref<VkFormat>		format,
	add_ref<Vec4>			dst,
	add_ref<Vec4>			src,
	add_ref<Vec4>			out)
{
	static UVec2 c2, c1, c0(INVALID_UINT32);
	if (INVALID_UINT32 == c0.x())
	{
		Vec2 p;
		std::vector<Vec2> vertices(bufferGetElementCount<Vec2>(vertexBuffer));
		bufferRead(vertexBuffer, vertices);

		p = barycenter(vertices.at(0), vertices.at(1), vertices.at(4));
		transformDistance(-1.0f, +1.0f, p.x(), 0u, extent.width, c0.x(), false);
		transformDistance(-1.0f, +1.0f, p.y(), 0u, extent.height, c0.y(), false);

		p = barycenter(vertices.at(8), vertices.at(2), vertices.at(7));
		transformDistance(-1.0f, +1.0f, p.x(), 0u, extent.width, c1.x(), false);
		transformDistance(-1.0f, +1.0f, p.y(), 0u, extent.height, c1.y(), false);

		p = barycenter(vertices.at(4), vertices.at(2), vertices.at(8));
		transformDistance(-1.0f, +1.0f, p.x(), 0u, extent.width, c2.x(), false);
		transformDistance(-1.0f, +1.0f, p.y(), 0u, extent.height, c2.y(), false);
	}

	std::variant<std::monostate,
		BufferTexelAccess<float>, BufferTexelAccess<Float16>,
		BufferTexelAccess<int32_t>, BufferTexelAccess<uint32_t>,
		BufferTexelAccess<int16_t>, BufferTexelAccess<uint16_t>,
		BufferTexelAccess<int8_t>, BufferTexelAccess<uint8_t>
	> v;
	
	const ZFormatInfo fi = formatGetInfo(format);
	const auto componentByteSize = fi.componentByteSizes[0];
	if (fi.floating)
	{
		if (componentByteSize == 4)
			v.emplace<1>(colorBuffer, extent.width * fi.componentCount, extent.height);
		else
			v.emplace<2>(colorBuffer, extent.width * fi.componentCount, extent.height);
	}
	else if (fi.pack)
	{
		switch (fi.pack)
		{
		case 32:
			v.emplace<4>(colorBuffer, extent.width, extent.height);
			break;
		case 16:
			v.emplace<6>(colorBuffer, extent.width, extent.height);
			break;
		case 8:
			v.emplace<8>(colorBuffer, extent.width, extent.height);
			break;
		default:
			ASSERTFALSE("Unknown format ", fi.name);
		}
	}
	else
	{
		if (componentByteSize == 4)
		{
			if (fi.isSigned)
				v.emplace<3>(colorBuffer, extent.width * fi.componentCount, extent.height);
			else
				v.emplace<4>(colorBuffer, extent.width * fi.componentCount, extent.height);
		}
		else if (componentByteSize == 2)
		{
			if (fi.isSigned)
				v.emplace<5>(colorBuffer, extent.width * fi.componentCount, extent.height);
			else
				v.emplace<6>(colorBuffer, extent.width * fi.componentCount, extent.height);
		}
		else
		{
			if (fi.isSigned)
				v.emplace<7>(colorBuffer, extent.width * fi.componentCount, extent.height);
			else
				v.emplace<8>(colorBuffer, extent.width * fi.componentCount, extent.height);
		}
	}

	std::visit([&](const auto& a) {
		if constexpr (std::negation_v<std::is_same<std::monostate, std::decay_t<decltype(a)>>>)
		{
			if (fi.pack)
			{
				dst = a.asColor(format, c0.x(), c0.y());
				src = a.asColor(format, c1.x(), c1.y());
				out = a.asColor(format, c2.x(), c2.y());
			}
			else
			{
				switch (fi.componentCount)
				{
				case 4:
					dst.a(a.asColor(format, (c0.x() * fi.componentCount) + 3, c0.y()).x());
					src.a(a.asColor(format, (c1.x() * fi.componentCount) + 3, c1.y()).x());
					out.a(a.asColor(format, (c2.x() * fi.componentCount) + 3, c2.y()).x());
					[[fallthrough]];
				case 3:
					dst.b(a.asColor(format, (c0.x() * fi.componentCount) + 2, c0.y()).x());
					src.b(a.asColor(format, (c1.x() * fi.componentCount) + 2, c1.y()).x());
					out.b(a.asColor(format, (c2.x() * fi.componentCount) + 2, c2.y()).x());
					[[fallthrough]];
				case 2:
					dst.g(a.asColor(format, (c0.x() * fi.componentCount) + 1, c0.y()).x());
					src.g(a.asColor(format, (c1.x() * fi.componentCount) + 1, c1.y()).x());
					out.g(a.asColor(format, (c2.x() * fi.componentCount) + 1, c2.y()).x());
					[[fallthrough]];
				case 1:
					dst.r(a.asColor(format, (c0.x() * fi.componentCount) + 0, c0.y()).x());
					src.r(a.asColor(format, (c1.x() * fi.componentCount) + 0, c1.y()).x());
					out.r(a.asColor(format, (c2.x() * fi.componentCount) + 0, c2.y()).x());
				}
			}
		}
	}, v);
}

class BlendEquation
{
	const VkPipelineColorBlendAttachmentState m_state;
	const Vec4 m_color0;
	const Vec4 m_color1;
	const Vec4 m_colorDst;
	const Vec4 m_colorOut;
	const Vec4 m_colorC;
	const float m_epsilon;
public:
	BlendEquation (add_cref<VkPipelineColorBlendAttachmentState> state, float epsilon, add_cref<Vec4> colorDst,
				   add_cref<Vec4> color0, add_cref<Vec4> color1, add_cref<Vec4> colorOut, add_cref<Vec4> colorC)
		: m_state		(state)
		, m_color0		(color0)
		, m_color1		(color1)
		, m_colorDst	(colorDst)
		, m_colorOut	(colorOut)
		, m_colorC		(colorC)
		, m_epsilon		(epsilon) {}
	std::string getText(add_ref<BoolVec4> matchingResult) const
	{
		std::ostringstream strSrcColorFactor, strDstColorFactor, res;
		Vec3 srcColorFactor, dstColorFactor, color2;
		const Vec3 mColorSrc0 = m_color0.cast<Vec3>();
		const Vec3 mColorSrc1 = m_color1.cast<Vec3>();
		const Vec3 mColorDst = m_colorDst.cast<Vec3>();
		const float mAlphaSrc0 = m_color0.a();
		const float mAlphaSrc1 = m_color1.a();
		const float mAlphaDst = m_colorDst.a();

		switch (m_state.srcColorBlendFactor)
		{
		case VK_BLEND_FACTOR_ZERO:
			srcColorFactor = Vec3();
			strSrcColorFactor << Vec3();
			break;
		case VK_BLEND_FACTOR_ONE:
			srcColorFactor = Vec3(1);
			strSrcColorFactor << Vec3(1);
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			srcColorFactor = mColorSrc0;
			strSrcColorFactor << mColorSrc0;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			srcColorFactor = Vec3(1) - mColorSrc0;
			strSrcColorFactor << '(' << Vec3(1) << " - " << mColorSrc0 << ')';
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			srcColorFactor = mColorDst;
			strSrcColorFactor << mColorDst;
			break;
		case VK_BLEND_FACTOR_SRC1_COLOR:
			srcColorFactor = mColorSrc1;
			strSrcColorFactor << mColorSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			srcColorFactor = Vec3(1) - mColorDst;
			strSrcColorFactor << '(' << Vec3(1) << " - " << mColorDst << ')';
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			srcColorFactor = Vec3(1) - mColorSrc1;
			strSrcColorFactor << '(' << Vec3(1) << " - " << mColorSrc1 << ')';
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			srcColorFactor = m_colorC.a();
			strSrcColorFactor << m_colorC.a();
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			srcColorFactor = (1.0f - m_colorC.a());
			strSrcColorFactor << "(1.0 - " << m_colorC.a() << ")";
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			srcColorFactor = mAlphaSrc0;
			strSrcColorFactor << mAlphaSrc0;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			srcColorFactor = (1.0f - mAlphaSrc0);
			strSrcColorFactor << "(1.0 - " << mAlphaSrc0 << ")";
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			srcColorFactor = mAlphaDst;
			strSrcColorFactor << mAlphaDst;
			break;
		case VK_BLEND_FACTOR_SRC1_ALPHA:
			srcColorFactor = mAlphaSrc1;
			strSrcColorFactor << mAlphaSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			srcColorFactor = (1 - mAlphaDst);
			strSrcColorFactor << "(1.0 - " << mAlphaDst << ")";
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
			srcColorFactor = (1 - mAlphaSrc1);
			strSrcColorFactor << "(1.0 - " << mAlphaSrc1 << ")";
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			srcColorFactor = Vec3(std::min(mAlphaSrc0, 1.0f - mAlphaDst));
			strSrcColorFactor << "min(" << Vec3(mAlphaSrc0) << ", " << Vec3(1.0f - mAlphaDst) << ")";
			break;
		default:
			break;
		}

		switch (m_state.dstColorBlendFactor)
		{
		case VK_BLEND_FACTOR_ZERO:
			dstColorFactor = Vec3();
			strDstColorFactor << Vec3();
			break;
		case VK_BLEND_FACTOR_ONE:
			dstColorFactor = Vec3(1);
			strDstColorFactor << Vec3(1);
			break;
		case VK_BLEND_FACTOR_SRC_COLOR:
			dstColorFactor = mColorSrc0;
			strDstColorFactor << mColorSrc0;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			dstColorFactor = Vec3(1) - mColorSrc0;
			strDstColorFactor << '(' << Vec3(1) << " - " << mColorSrc0 << ')';
			break;
		case VK_BLEND_FACTOR_DST_COLOR:
			dstColorFactor = mColorDst;
			strDstColorFactor << mColorDst;
			break;
		case VK_BLEND_FACTOR_SRC1_COLOR:
			dstColorFactor = mColorSrc1;
			strDstColorFactor << mColorSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			dstColorFactor = Vec3(1) - mColorDst;
			strDstColorFactor << '(' << Vec3(1) << " - " << mColorDst << ')';
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			dstColorFactor = Vec3(1) - mColorSrc1;
			strDstColorFactor << '(' << Vec3(1) << " - " << mColorSrc1 << ')';
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			dstColorFactor = m_colorC.a();
			strDstColorFactor << m_colorC.a();
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			dstColorFactor = (1.0f - m_colorC.a());
			strDstColorFactor << "(1.0 - " << m_colorC.a();
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
			dstColorFactor = mAlphaSrc0;
			strDstColorFactor << mAlphaSrc0;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			dstColorFactor = (1.0f - mAlphaSrc0);
			strDstColorFactor << "(1.0 - " << mAlphaSrc0 << ')';
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
			dstColorFactor = mAlphaDst;
			strDstColorFactor << mAlphaDst;
			break;
		case VK_BLEND_FACTOR_SRC1_ALPHA:
			dstColorFactor = mAlphaSrc1;
			strDstColorFactor << mAlphaSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			dstColorFactor = (1.0f - mAlphaDst);
			strDstColorFactor << "(1.0 - " << mAlphaDst << ")";
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
			dstColorFactor = (1.0f - mAlphaSrc1);
			strDstColorFactor << "(1.0 - " << mAlphaSrc1 << ")";
			break;
		default:
			break;
		}

		switch (m_state.colorBlendOp)
		{
		case VK_BLEND_OP_ADD:
			color2 = srcColorFactor * mColorSrc0 + dstColorFactor * mColorDst;
			res << strSrcColorFactor.str() << " * " << mColorSrc0 << " + " << strDstColorFactor.str() << " * " << mColorDst;
			res << " = " << (srcColorFactor * mColorSrc0) << " + " << (dstColorFactor * mColorDst);
			break;
		case VK_BLEND_OP_SUBTRACT:
			color2 = srcColorFactor * mColorSrc0 - dstColorFactor * mColorDst;
			res << strSrcColorFactor.str() << " * " << mColorSrc0 << " - " << strDstColorFactor.str() << " * " << mColorDst;
			res << " = " << (srcColorFactor * mColorSrc0) << " - " << (dstColorFactor * mColorDst);
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			color2 = dstColorFactor * mColorDst - srcColorFactor * mColorSrc0;
			res << strDstColorFactor.str() << " * " << mColorDst << " - " << strSrcColorFactor.str() << " * " << mColorSrc0;
			res << " = " << (dstColorFactor * mColorDst) << " - " << (srcColorFactor * mColorSrc0);
			break;
		case VK_BLEND_OP_MIN:
			color2 = (mColorSrc0).min(mColorDst);
			res << "min(" << mColorSrc0 << ", " << mColorDst << ")";
			break;
		case VK_BLEND_OP_MAX:
			color2 = (mColorSrc0).max(mColorDst);
			res << "max(" << mColorSrc0 << ", " << mColorDst << ")";
			break;
		default:
			break;
		}

		float srcAlphaFactor = 0.0f, dstAlphaFactor = 0.0f;
		std::ostringstream strSrcAlphaFactor, strDstAlphaFactor;

		switch (m_state.srcAlphaBlendFactor)
		{
		case VK_BLEND_FACTOR_ZERO:
			srcAlphaFactor = 0.0f; strSrcAlphaFactor << "0.0";
			break;
		case VK_BLEND_FACTOR_ONE:
			srcAlphaFactor = 1.0f; strSrcAlphaFactor << "1.0";
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
		case VK_BLEND_FACTOR_SRC_COLOR:
			srcAlphaFactor = mAlphaSrc0; strSrcAlphaFactor << mAlphaSrc0;
			break;
		case VK_BLEND_FACTOR_SRC1_ALPHA:
		case VK_BLEND_FACTOR_SRC1_COLOR:
			srcAlphaFactor = mAlphaSrc1; strSrcAlphaFactor << mAlphaSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			srcAlphaFactor = 1.0f - mAlphaSrc0; strSrcAlphaFactor << "(1.0 - " << mAlphaSrc0 << ")";
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			srcAlphaFactor = 1.0f - mAlphaSrc1; strSrcAlphaFactor << "(1.0 - " << mAlphaSrc1 << ")";
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
		case VK_BLEND_FACTOR_DST_COLOR:
			srcAlphaFactor = mAlphaDst; strSrcAlphaFactor << mAlphaDst;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			srcAlphaFactor = 1.0f - mAlphaDst; strSrcAlphaFactor << "(1.0 - " << mAlphaDst << ")";
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
			srcAlphaFactor = std::min(mAlphaSrc0, 1.0f - mAlphaDst);
			strSrcAlphaFactor << "min(" << mAlphaSrc0 << ", 1.0 - " << mAlphaDst << ")";
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			srcAlphaFactor = m_colorC.a(); strSrcAlphaFactor << m_colorC.a();
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			srcAlphaFactor = 1.0f - m_colorC.a(); strSrcAlphaFactor << "(1.0 - " << m_colorC.a() << ")";
			break;
		default:
			break;
		}

		switch (m_state.dstAlphaBlendFactor)
		{
		case VK_BLEND_FACTOR_ZERO:
			dstAlphaFactor = 0.0f; strDstAlphaFactor << "0.0";
			break;
		case VK_BLEND_FACTOR_ONE:
			dstAlphaFactor = 1.0f; strDstAlphaFactor << "1.0";
			break;
		case VK_BLEND_FACTOR_SRC_ALPHA:
		case VK_BLEND_FACTOR_SRC_COLOR:
			dstAlphaFactor = mAlphaSrc0; strDstAlphaFactor << mAlphaSrc0;
			break;
		case VK_BLEND_FACTOR_SRC1_ALPHA:
		case VK_BLEND_FACTOR_SRC1_COLOR:
			dstAlphaFactor = mAlphaSrc1; strDstAlphaFactor << mAlphaSrc1;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			dstAlphaFactor = 1.0f - mAlphaSrc0; strDstAlphaFactor << "(1.0 - " << mAlphaSrc0 << ")";
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			dstAlphaFactor = 1.0f - mAlphaSrc1; strDstAlphaFactor << "(1.0 - " << mAlphaSrc1 << ")";
			break;
		case VK_BLEND_FACTOR_DST_ALPHA:
		case VK_BLEND_FACTOR_DST_COLOR:
			dstAlphaFactor = mAlphaDst; strDstAlphaFactor << mAlphaDst;
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			dstAlphaFactor = 1.0f - mAlphaDst; strDstAlphaFactor << "(1.0 - " << mAlphaDst << ")";
			break;
		case VK_BLEND_FACTOR_CONSTANT_ALPHA:
			dstAlphaFactor = m_colorC.a(); strDstAlphaFactor << m_colorC.a();
			break;
		case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
			dstAlphaFactor = 1.0f - m_colorC.a(); strDstAlphaFactor << "(1.0 - " << m_colorC.a() << ")";
			break;
		default:
			break;
		}

		float alphaResult = 0.0f;
		std::ostringstream resAlpha;
		switch (m_state.alphaBlendOp)
		{
		case VK_BLEND_OP_ADD:
			alphaResult = srcAlphaFactor * mAlphaSrc0 + dstAlphaFactor * mAlphaDst;
			resAlpha << strSrcAlphaFactor.str() << " * " << mAlphaSrc0 << " + " << strDstAlphaFactor.str() << " * " << mAlphaDst;
			break;
		case VK_BLEND_OP_SUBTRACT:
			alphaResult = srcAlphaFactor * mAlphaSrc0 - dstAlphaFactor * mAlphaDst;
			resAlpha << strSrcAlphaFactor.str() << " * " << mAlphaSrc0 << " - " << strDstAlphaFactor.str() << " * " << mAlphaDst;
			break;
		case VK_BLEND_OP_REVERSE_SUBTRACT:
			alphaResult = dstAlphaFactor * mAlphaDst - srcAlphaFactor * mAlphaSrc0;
			resAlpha << strDstAlphaFactor.str() << " * " << mAlphaDst << " - " << strSrcAlphaFactor.str() << " * " << mAlphaSrc0;
			break;
		case VK_BLEND_OP_MIN:
			alphaResult = std::min(srcAlphaFactor, dstAlphaFactor);
			resAlpha << "min(" << strSrcAlphaFactor.str() << ", " << strDstAlphaFactor.str() << ")";
			break;
		case VK_BLEND_OP_MAX:
			alphaResult = std::max(srcAlphaFactor, dstAlphaFactor);
			resAlpha << "max(" << strSrcAlphaFactor.str() << ", " << strDstAlphaFactor.str() << ")";
			break;
		default:
			break;
		}

		std::ostringstream eq;

		const Vec3 mColorOut = m_colorOut.cast<Vec3>();
		const auto colorMatches = color2.matches(mColorOut, DVec3(m_epsilon));
		eq << "Color: " << res.str() << " = " << color2 << "; screen: " << mColorOut;
		eq << ", " << ((colorMatches == BoolVec3(true)) ? " matches" : " DOESN'T MATCH");
		eq << std::endl;

		const bool alphaMatches = std::abs(alphaResult - m_colorOut.a()) < m_epsilon;
		eq << "Alpha: " << resAlpha.str() << " = " << alphaResult << "; screen: " << m_colorOut.a();
		eq << ", " << (alphaMatches ? " matches" : " DOESN'T MATCH");
		eq << std::endl;

		const BoolVec4 fullMatches(colorMatches.x(), colorMatches.y(), colorMatches.z(), alphaMatches);
		const Vec4 colorResult(color2.r(), color2.g(), color2.b(), alphaResult);
		eq << "Color comparison result: [";
		eq << boolean(fullMatches.r()) << ", "	<< boolean(fullMatches.g()) << ", "
			<< boolean(fullMatches.b()) << ", " << boolean(fullMatches.a()) << ']';
		//eq << ", " << (colorMatchesState ? " matches" : " DOESN'T MATCH");

		matchingResult = fullMatches;

		return eq.str();
	}
};

TriLogicInt runTests (add_ref<Canvas> ctx, add_cref<std::string> assets,
						add_cref<std::vector<TestParamsState>> set, bool fromFile,
						bool availableDualSourceBlend, bool needShaderObject)
{
	add_cref<ZDeviceInterface>	di = ctx.device.getInterface();

	ZShaderModule				commonVert;
	ZShaderModule				genericFrag;
	ZShaderModule				dualFrag;
	std::tie(commonVert, genericFrag, dualFrag) = buildProgram(ctx.device, assets, availableDualSourceBlend);

	struct PushConstant
	{
		Vec4	 fColor0;
		Vec4	 fColor1;
		Vec4	 fColor2;
	}							pc;

	ZShaderObject				soCommonVert;
	ZShaderObject				soGenericFrag;
	ZShaderObject				soDualFrag;
	if (needShaderObject)
	{
		std::tie(soCommonVert, soGenericFrag, soDualFrag) =
			buildProgram<PushConstant>(ctx.device, assets, availableDualSourceBlend);
	}

	ZBuffer						vertexBuffer;
	ZBuffer						indexBuffer;
	VertexInput					vertexInput	(ctx.device);
	std::tie(vertexBuffer, indexBuffer) = genVertices(vertexInput);

	LayoutManager				lm				(ctx.device);
	ZPipelineLayout				colorLayout		= lm.createPipelineLayout(ZPushRange<PushConstant>());

	ZRenderPass					colorRenderPass;
	ZImage						colorImage;
	ZImageView					colorView;
	gpp::Attachment				colorAttachment;
	ZFramebuffer				colorFB;
	ZBuffer						colorBuffer;
	ZPipeline					backgroundPipeline;
	ZPipeline					blendPipeline;
	ZShaderObject				backgroundShader;
	ZShaderObject				blendShader;

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

		if (lastTextIndex != ev.testCounter)
		{
			lastTextIndex = ev.testCounter;
			if (fromFile)
			{
				std::cout << "==============" << std::endl;
				std::cout << "Perform test: " << ev.testCounter << std::endl;
				std::cout << "Command line: " << std::get<2>(test) << std::endl;
			}
			currParams.print(std::cout, availableDualSourceBlend, std::get<1>(test));
			//printFlatnizeWarning(std::cout, currParams, availableDualSourceBlend);
		}

		const VkFormat colorFormat = VkFormat(currParams.colorFormat);
		colorImage			= ctx.createColorImage2D(colorFormat, TestParams::defaultExtent);
		colorView			= createImageView(colorImage);
		colorAttachment		= gpp::Attachment(colorView, gpp::AttachmentDesc::Color);
		colorRenderPass		= createColorRenderPass(ctx.device, { colorFormat }, {}, VK_IMAGE_LAYOUT_GENERAL);
		colorFB				= createFramebuffer(colorRenderPass, TestParams::defaultExtent, { colorView });
		colorBuffer			= createBuffer(colorImage, ZBufferUsageStorageFlags, ZMemoryPropertyHostFlags);

		if (currParams.enableShaderObject)
		{
			backgroundShader = soGenericFrag;
			blendShader = useDualBlend ? soDualFrag : soGenericFrag;
		}
		else
		{
			backgroundPipeline = createGraphicsPipeline(colorLayout, colorRenderPass,
				TestParams::defaultExtent, vertexInput, commonVert, genericFrag);
			blendPipeline = makeBlendPipeline(test, colorLayout, colorRenderPass, vertexInput,
				commonVert, genericFrag, dualFrag, useDualBlend);
		}

		pc.fColor0 = currParams.dstColor;
		pc.fColor1 = currParams.srcColor0;
		pc.fColor2 = currParams.srcColor1;
		bufferFill<Vec4>(colorBuffer, Vec4());

		ZImage				displayImage	= framebufferGetImage(displayFB);
		ZImageMemoryBarrier	srcPreBlit		(colorImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstPreBlit		(displayImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
															VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(displayCmd);
			commandBufferClearColorImage(displayCmd, colorImage, makeClearColorValue(Vec4()));
			commandBufferBindIndexBuffer(displayCmd, indexBuffer);
			commandBufferBindVertexBuffers(displayCmd, vertexInput);
			commandBufferPushConstants(displayCmd, colorLayout, pc);
			{
				if (currParams.enableShaderObject)
				{
					commandBufferSetDefaultDynamicStates(displayCmd, vertexInput, makeViewport(currParams.defaultExtent));
					commandBufferBindShaders(displayCmd, { soCommonVert, backgroundShader });
					commandBufferBeginRendering(displayCmd,
												TestParams::defaultExtent.width,
												TestParams::defaultExtent.height, { colorAttachment });
					vkCmdDrawIndexed(*displayCmd, 12, 1, 0, 0, 0);  // dst color sample
					vkCmdDrawIndexed(*displayCmd, 12, 1, 18, 0, 1); // src color sample
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 0);  // dst color blending area
					if (useDualBlend) { vkCmdDrawIndexed(*displayCmd, 6, 1, 30, 0, 2); } // src1 color sample
					commandBufferEndRendering(displayCmd);
				}
				else
				{
					commandBufferBindPipeline(displayCmd, backgroundPipeline);
					auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
					vkCmdDrawIndexed(*displayCmd, 12, 1, 0, 0, 0);  // dst color sample
					vkCmdDrawIndexed(*displayCmd, 12, 1, 18, 0, 1); // src color sample
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 0);  // dst color blending area
					if (useDualBlend) { vkCmdDrawIndexed(*displayCmd, 6, 1, 30, 0, 2); } // src1 color sample
					commandBufferEndRenderPass(rpbi);
				}
			}
			{
				ZImageMemoryBarrier srcColorReady(colorImage, VK_ACCESS_NONE, VK_ACCESS_NONE,
													VK_IMAGE_LAYOUT_GENERAL);

				commandBufferPipelineBarriers(displayCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
														  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
														  srcColorReady);
				if (currParams.enableShaderObject)
				{
					add_cref<VkExtent3D> colorSize = imageGetExtent(colorImage);
					const VkPipelineColorBlendAttachmentState state = currParams.getState();
					commandBufferSetDefaultDynamicStates(displayCmd, vertexInput, makeViewport(currParams.defaultExtent));
					di.vkCmdSetColorBlendEnableEXT(*displayCmd, 0u, 1u, makeQuickPtr(VK_TRUE));
					di.vkCmdSetColorBlendEquationEXT(*displayCmd, 0u, 1u, makeQuickPtr(makeColorBlendEquationExt(state)));
					di.vkCmdSetColorWriteMaskEXT(*displayCmd, 0u, 1u, &state.colorWriteMask);
					di.vkCmdSetBlendConstants(*displayCmd, currParams.constColor.getData());
					commandBufferBindShaders(displayCmd, { soCommonVert, blendShader });
					commandBufferBeginRendering(displayCmd, colorSize.width, colorSize.height, { colorAttachment });
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 1);
					commandBufferEndRendering(displayCmd);
				}
				else
				{
					commandBufferBindPipeline(displayCmd, blendPipeline);
					auto rpbi = commandBufferBeginRenderPass(displayCmd, colorFB, 0);
					vkCmdDrawIndexed(*displayCmd, 6, 1, 12, 0, 1);
					commandBufferEndRenderPass(rpbi);
				}
			}

			imageCopyToBuffer(displayCmd, colorImage, colorBuffer,
								VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
								VK_ACCESS_NONE, VK_ACCESS_NONE,
								VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT);

			commandBufferPipelineBarriers(displayCmd,
											VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											srcPreBlit, dstPreBlit);
			commandBufferBlitImage(displayCmd, colorImage, displayImage, VK_FILTER_NEAREST);
			commandBufferMakeImagePresentationReady(displayCmd, displayImage);
		commandBufferEnd(displayCmd);
	};

	ZRenderPass swapRenderPass = createColorRenderPass(ctx.device, { ctx.surfaceFormat }, {}, VK_IMAGE_LAYOUT_MAX_ENUM);

	std::vector<BoolVec4> results(data_count(set));
	auto onAfterRecording = [&](add_ref<Canvas>) -> void
	{
		const uint32_t				testIndex	(ev.testCounter);
		add_cref<TestParamsState>	test		(set.at(testIndex));
		add_cref<TestParams>		currParams	(std::get<0>(test));
		const VkFormat				colorFormat = VkFormat(currParams.colorFormat);

		const uint32_t dataSize = bufferGetElementCount<float>(colorBuffer);
		std::vector<float> data(dataSize);
		bufferRead(colorBuffer, data);

		Vec4 dstColor, srcColor, outColor;
		readColors(vertexBuffer, colorBuffer, TestParams::defaultExtent,
					colorFormat, dstColor, srcColor, outColor);

		const BlendEquation beq(currParams.getState(), currParams.epsilon,
								dstColor, srcColor, currParams.srcColor1, outColor, currParams.constColor);
		std::cout << beq.getText(results[testIndex]) << std::endl;
	};

	ctx.run(onCommandRecording, swapRenderPass, std::ref(ev.swapTrigger), {/*onIdle()*/}, onAfterRecording);

	uint32_t failCount = 0u;
	for (add_cref<BoolVec4> res : results)
	{
		if (false == (res.a() && res.g() && res.b() && res.a()))
			failCount = failCount + 1u;
	}

	if (failCount)
	{
		std::cout << "Result: " << failCount << " test(s) failed from " << data_count(set) << std::endl;
		return 1;
	}

	std::cout << "Result: " << data_count(set) << " test(s) pass\n";
	return 0;
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


