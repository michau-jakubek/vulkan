#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfVector.hpp"
#include "vtfStructUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "subgroupMatrixTests.hpp"

#include <array>
#include <charconv>
#include <cstring>
#include <functional>
#include <random>
#include <string_view>
#include <stdio.h>

#include "vtfZSpecializationInfo.hpp"

namespace
{
using namespace vtf;
typedef add_ref<std::ostream>	ostream_ref;
typedef std::vector<uint32_t>	u32vec;
typedef add_ref<u32vec>			u32vec_ref;
typedef add_cref<u32vec>		u32vec_cref;

uint32_t getSystemSubgroupSize (ZDevice device)
{
	VkPhysicalDeviceSubgroupProperties	sp	= makeVkStruct();
	VkPhysicalDeviceProperties2			p2	= makeVkStruct(&sp);
	vkGetPhysicalDeviceProperties2(*device.getParam<ZPhysicalDevice>(), &p2);
	return sp.subgroupSize;
}

std::tuple<UVec3,UVec3,uint32_t,uint32_t,uint32_t> getDeviceLimits (ZDevice device)
{
	add_cref<VkPhysicalDeviceLimits> limits = deviceGetPhysicalLimits(device);

	const UVec3		maxComputeWorkGroupCount	(limits.maxComputeWorkGroupCount[0],
												 limits.maxComputeWorkGroupCount[1],
												 limits.maxComputeWorkGroupCount[2]);
	const UVec3		maxComputeWorkGroupSize		(limits.maxComputeWorkGroupSize[0],
												 limits.maxComputeWorkGroupSize[1],
												 limits.maxComputeWorkGroupSize[2]);
	// VkPhysicalDeviceVulkan13Properties
	// VkPhysicalDeviceSubgroupSizeControlProperties
	// const uint32_t	maxComputeWorkgroupSubgroups	(limits.maxComputeWorkgroupSubgroups);
	const uint32_t	maxComputeWorkGroupInvocations	(limits.maxComputeWorkGroupInvocations);
	const uint32_t	gl_SubgroupSize					(getSystemSubgroupSize(device));
	const uint32_t	maxComputeSharedMemorySize		(limits.maxComputeSharedMemorySize);

	return { maxComputeWorkGroupCount, maxComputeWorkGroupSize, /*maxComputeWorkgroupSubgroups,*/
				maxComputeWorkGroupInvocations, gl_SubgroupSize, maxComputeSharedMemorySize };
}

struct Params
{
	enum class Status
	{
		OK, Help, Limits, Error, Warning
	};
	enum InputTypes
	{
		Uint32	= 0,
		Int32	= 1,
		Float32	= 2
	};
	enum class AddresingMode : uint32_t
	{
		local, absolut
	};

	UVec3					localSize;
	UVec3					workGroup;
	const uint32_t			subgroupSize;
	uint32_t				checkpointMax;
	uint32_t				printColCount;
	uint32_t				printRowCount;
	AddresingMode			addressingMode;
	uint32_t				providedValueCount;
	const uint32_t			paramsBufferLength;
	const uint32_t			controlIndex0;	// subgroupSize / 3
	const uint32_t			controlIndex1;	// subgroupSize - 2
	const uint32_t			controlIndex2;	// subgroupSize - 1
	const uint32_t			controlIndex3;	// subgroupSize
	const uint32_t			controlIndex4;	// subgroupSize + 1
	mutable std::vector<uint32_t>	inputIvalues;
	mutable std::vector<uint32_t>	inputJvalues;
	mutable std::vector<uint32_t>	inputTypes;
	std::vector<UVec2>				specConstants;
	struct {
		uint32_t printParams	: 1;	// print params from cmdLine and continue
		uint32_t printDesc		: 1;	// print description and exits
		uint32_t printInHex		: 1;	// print numbers in hexadecimal format
		uint32_t printCapital	: 1;	// print capital characters
		uint32_t printZero		: 1;	// print zero in result
		uint32_t noPrintResult	: 1;	// don't print result
		uint32_t enableSUCF		: 1;	// enable subgroup_uniform_control_flow extension
		uint32_t noBuildAlways	: 1;	// disable automatic shader builing
	}
	flags {
		// designated initializers are a C++20 extension [-Werror,-Wc++20-designator]
		0, // printParams
		0, // printDesc
		0, // printHex
		0, // printCapital
		0, // printZero
		0, // noPrintResult
		0, // enableSUCF
		0, // noAlwaysBuild
	};
	void		print (add_ref<std::ostream> log) const;
	bool		verify (ZDevice device, add_ref<std::ostream> log) const;
	void		usage (ZDevice device, add_ref<std::ostream> log, add_cref<std::string> assets) const;
	add_cptr<char> static addressingModeToString (AddresingMode am);
	std::tuple<Params::Status, Params, std::string>
	static parseCommandLine (ZDevice device, add_cref<strings> cmdLineParams);
	Params (uint32_t inSubgroupSize);
	struct StreamThrough
	{
		add_cref<Params>	me;
		uint32_t			val;
		uint32_t			at;
		uint32_t			sto;
	};
	StreamThrough printInputValue (uint32_t at, uint32_t storage) const;
	StreamThrough printInputValue (uint32_t value, uint32_t at, uint32_t storage) const;
	friend std::ostream& operator<<(std::ostream& str, const StreamThrough& streamThrough);

private:
	void setInputType (u32vec_cref values, const uint32_t at, InputTypes type);
	template<class X> void setInput (const X& val, const uint32_t at, u32vec_ref values);
	template<class X> void setInput (u32vec_ref values, const X& a, const X& b);
	void printInputValueImpl (add_ref<std::ostream> str, add_cref<StreamThrough> streamThrough) const;
};
Params::Params (uint32_t inSubgroupSize)
	: localSize				(32u, 1u, 1u)
	, workGroup				(1)
	, subgroupSize			(inSubgroupSize)
	, checkpointMax			(20)
	, printColCount			(8)
	, printRowCount			(16)
	, addressingMode		(AddresingMode::local)
	, providedValueCount	(0)
	, paramsBufferLength	(67)
	, controlIndex0			(inSubgroupSize / 3u)
	, controlIndex1			(inSubgroupSize - 2u)
	, controlIndex2			(inSubgroupSize - 1u)
	, controlIndex3			(inSubgroupSize)
	, controlIndex4			(inSubgroupSize + 1u)
	, inputIvalues			()
	, inputJvalues			()
	, inputTypes			()
{
}
Params::StreamThrough Params::printInputValue (uint32_t at, uint32_t storage) const
{
	return printInputValue(INVALID_UINT32, at, storage);
}
Params::StreamThrough Params::printInputValue (uint32_t val, uint32_t at, uint32_t storage) const
{
	return { *this, val, at, storage };
}
inline std::ostream& operator<<(std::ostream& str, const Params::StreamThrough& streamThrough)
{
	streamThrough.me.printInputValueImpl(str, streamThrough);
	return str;
}
void Params::printInputValueImpl (add_ref<std::ostream> str, add_cref<StreamThrough> streamThrough) const
{
	add_cptr<std::vector<uint32_t>> p = nullptr;
	if (streamThrough.sto == 0u)
	{
		p = &inputIvalues;
	}
	else if (streamThrough.sto == 1u)
	{
		p = &inputJvalues;
	}
	else { ASSERTION(0); }
	u32vec_cref inputValues(*p);
	const uint32_t inputValue = (streamThrough.val == INVALID_UINT32)
							? inputValues.at(streamThrough.at)
							: streamThrough.val;
	switch ((InputTypes)((uint16_t const*)&inputTypes.at(streamThrough.at))[streamThrough.sto])
	{
	case Uint32:	str << std::setw(8) << inputValue << std::setw(0); break;
	case Int32:		str << std::setw(8) << make_signed(inputValue) << std::setw(0); break;
	case Float32:	str << std::setw(8) << UVec1(inputValue).bitcast<Vec1>().x() << std::setw(0); break;
	default:		str << "<unk>" << std::setw(3) << inputValue << std::setw(0); break;
	}
}
void Params::setInputType (u32vec_cref values, const uint32_t at, InputTypes type)
{
	// looks like stupid but only in test purpose
	uint32_t typeIdx = 0u;
	if (&values == &inputIvalues)
		typeIdx = 0u;
	else if (&values == &inputJvalues)
		typeIdx = 1u;
	else { ASSERTION(0); }
	if (at != INVALID_UINT32)
		((uint16_t*)(&inputTypes.at(at)))[typeIdx] = uint16_t(type);
	else
	{
		for (add_ref<uint32_t> i : inputTypes)
			((uint16_t*)&i)[typeIdx] = uint16_t(type);
	}
}
template<> void Params::setInput<int32_t> (const int32_t& val, uint32_t at, u32vec_ref values)
{
	setInputType(values, at, Int32);
	values.at(at) = make_unsigned(val);
}
template<> void Params::setInput<uint32_t> (const uint32_t& val, uint32_t at, u32vec_ref values)
{
	setInputType(values, at, Uint32);
	values.at(at) = val;
}
template<> void Params::setInput<float> (const float& val, uint32_t at, u32vec_ref values)
{
	setInputType(values, at, Float32);
	values.at(at) = Vec1(val).bitcast<UVec1>().x();
}
template<> void Params::setInput<int32_t>(u32vec_ref values, const int32_t& a, const int32_t& b)
{
	setInputType(values, INVALID_UINT32, Int32);
	if (a == b)
	{
		for (add_ref<uint32_t> i : values)
			i = make_unsigned(a);
	}
	else
	{
		std::random_device						rd;
		std::mt19937							gen(rd());
		std::uniform_int_distribution<int32_t>	dis(a, b);
		for (add_ref<uint32_t> i : values)
			i = make_unsigned(dis(gen));
	}
}
template<> void Params::setInput<uint32_t> (u32vec_ref values, const uint32_t& a, const uint32_t& b)
{
	setInputType(values, INVALID_UINT32, Uint32);
	if (a == b)
	{
		for (add_ref<uint32_t> i : values)
			i = a;
	}
	else
	{
		std::random_device						rd;
		std::minstd_rand0						gen(rd());
		std::uniform_int_distribution<uint32_t> dis(a, b);
		for (add_ref<uint32_t> i : values)
			i = dis(gen);
	}
}
template<> void Params::setInput<float> (u32vec_ref values, const float& a, const float& b)
{
	setInputType(values, INVALID_UINT32, Float32);
	if (a == b)
	{
		for (add_ref<uint32_t> i : values)
			i = Vec1(a).bitcast<UVec1>().x();
	}
	else
	{
		std::random_device						rd;
		std::mt19937							gen(rd());
		std::uniform_real_distribution<float>	dis(a, b);
		for (add_ref<uint32_t> i : values)
			i = Vec1(dis(gen)).bitcast<UVec1>().x();
	}
}
bool Params::verify (ZDevice device, add_ref<std::ostream> log) const
{
	bool result = true;
	auto logError = [&](add_cptr<char> lName, const auto& lValue, add_cptr<char> rName, const auto& rValue)
	{
		log << "[ERROR] " << lName << lValue << " must not exceed " << rName << rValue << std::endl;
	};
	const auto [maxComputeWorkGroupCount, maxComputeWorkGroupSize,
				maxComputeWorkGroupInvocations, gl_SubgroupSize, maxComputeSharedMemorySize] = getDeviceLimits(device);

	if (	workGroup.x() > maxComputeWorkGroupCount.x()
		||	workGroup.y() > maxComputeWorkGroupCount.y()
		||	workGroup.z() > maxComputeWorkGroupCount.z())
	{
		result = false;
		logError(STRINGIZE(workGroup), workGroup, STRINGIZE(maxComputeWorkGroupCount), maxComputeWorkGroupCount);
	}
	if (	localSize.x() > maxComputeWorkGroupSize.x()
		||	localSize.y() > maxComputeWorkGroupSize.y()
		||	localSize.z() > maxComputeWorkGroupSize.z())
	{
		result = false;
		logError(STRINGIZE(localSize), localSize, STRINGIZE(maxComputeWorkGroupSize), maxComputeWorkGroupSize);
	}
	if (localSize.prod() > maxComputeWorkGroupInvocations)
	{
		result = false;
		logError("Product of " STRINGIZE(localSize), UVec1(localSize.prod()),
				 STRINGIZE(maxComputeWorkGroupInvocations), UVec1(maxComputeWorkGroupInvocations));
	}
	UNREF(maxComputeSharedMemorySize);
	UNREF(gl_SubgroupSize);

	return result;
}
void Params::usage (ZDevice device, add_ref<std::ostream> log, add_cref<std::string> assets) const
{
	bool status = true;
	const std::string usageFile = (fs::path(assets) / "usage.txt").string();
	const std::string formattedContent = readFile(usageFile, &status);
	if (false == status)
	{
		log << "[WARNING] Unable to open " << std::quoted(usageFile) << std::endl;
		return;
	}
	const Params def(getSystemSubgroupSize(device));
	const string_to_string_map abbreviations
	{
		{	"WX",	std::to_string(def.workGroup.x())	},
		{	"WY",	std::to_string(def.workGroup.y())	},
		{	"WZ",	std::to_string(def.workGroup.z())	},
		{	"LX",	std::to_string(def.localSize.x())	},
		{	"LY",	std::to_string(def.localSize.y())	},
		{	"LZ",	std::to_string(def.localSize.z())	},
		{	"CM",	std::to_string(def.checkpointMax)	},
		{	"CC",	std::to_string(def.printColCount)	},
		{	"RC",	std::to_string(def.printRowCount)	},
		{	"AddresingMode::local",		addressingModeToString(AddresingMode::local)	},
		{	"AddresingMode::absolut",	addressingModeToString(AddresingMode::absolut)	},
	};
	log << subst_variables(formattedContent, abbreviations, false);
}
void Params::print (add_ref<std::ostream> log) const
{
	const uint32_t numSubgroups = MULTIPLERUP(localSize.prod(), subgroupSize);
	const uint32_t inputValuesCount = data_count(inputIvalues);
	ASSERTION(inputValuesCount == data_count(inputJvalues));
	ASSERTION(inputValuesCount == data_count(inputTypes));
	log << "workGroup:          " << workGroup << std::endl
		<< "localSize:          " << localSize << std::endl
		<< "inputInvocationMax: " << (localSize.prod() - 1u) << std::endl
		<< "numSubgroups:       " << numSubgroups << std::endl
		<< "subgroupSize:       " << subgroupSize << std::endl
		<< "addressingMode:     " << addressingModeToString(addressingMode) << std::endl
		<< "checkpointMax:      " << checkpointMax << std::endl
		<< "inputValuesCount:   " << inputValuesCount << std::endl
		<< "paramsBufferLength: " << paramsBufferLength << std::endl
		<< "providedValueCount: " << providedValueCount << std::endl
		<< "printColCount:      " << printColCount << std::endl
		<< "printRowCount:      " << printRowCount << std::endl;

	for (const auto& p : { std::make_pair(0u, 'I'), std::make_pair(1u, 'J') })
	{
		bool atLeastOne = false;
		log << "inputValues " << p.second << ": ";
		for (const uint32_t i : { controlIndex0, controlIndex1, controlIndex2, controlIndex3, controlIndex4 })
		{
			if (i >= inputValuesCount) continue;
			if (atLeastOne) log << ", ";
			log << '[' << i << "]:" << printInputValue(i, p.first);
			atLeastOne = true;
		}
		log << std::endl;
	}
}
add_cptr<char> Params::addressingModeToString (AddresingMode am)
{
	switch (am)
	{
	case AddresingMode::absolut:	return "absolut";
	case AddresingMode::local:		return "local";
	}
	ASSERTMSG(false, "Unknown AddressingMode");
	return nullptr;
}
template<class C> using add_cref2 = typename std::add_const<typename std::add_lvalue_reference<C>::type>::type;
std::tuple<Params::Status, Params, std::string> Params::parseCommandLine(ZDevice device, add_cref<strings> cmdLineParams)
{
	bool status;
	strings sink;
	strings args(cmdLineParams);
	std::vector<Option> options;
	std::stringstream errorMessage;
	Params resultParams(getSystemSubgroupSize(device));

	Option optHelpShort	{ "-h", 0 };
	Option optHelpLong	{ "--help", 0 };
	Option optLimits	{ "--print-device-limits", 0 };
	container_push_back_more(options, { optHelpShort, optHelpLong, optLimits });
	if (cmdLineParams.empty()
		|| (consumeOptions(optHelpShort, options, args, sink) > 0)
		|| (consumeOptions(optHelpLong, options, args, sink) > 0))
	{
		return { Params::Status::Help, resultParams, std::string() };
	}
	if (consumeOptions(optLimits, options, args, sink) > 0)
	{
		return { Params::Status::Limits, resultParams, std::string() };
	}

	bool hasErrors = false;
	bool hasWarnings = false;
	auto appendMessage = [&](add_cref<Option> opt, add_cref<std::string> val, const auto& def, bool error = false) -> void
	{
		*(error ? &hasErrors : &hasWarnings) = true;
		errorMessage << (error ? "[ERROR] " : "[WARNING] ");
		errorMessage << "Unable to parse \'" << opt.name << " from \'" << val << "\' "
			<< ", default value \'" << def << "\' was used" << std::endl;
	};

	// Regular options
	if (true)
	{
		Option optWorkGroupX	{ "-wx", 1 };
		Option optWorkGroupY	{ "-wy", 1 };
		Option optWorkGroupZ	{ "-wz", 1 };
		Option optLocalSizeX	{ "-lx", 1 };
		Option optLocalSizeY	{ "-ly", 1 };
		Option optLocalSizeZ	{ "-lz", 1 };
		Option optCheckpointMax	{ "-cm", 1 };
		Option optPrintColCnt	{ "-cols", 1 };
		Option optPrintRowCnt	{ "-rows", 1 };
		Option optAddressMode	{ "-am", 1 };
		Option optPrintParams	{ "--print-params", 0 };
		Option optNoPrintRes	{ "--no-print-result", 0 };
		Option optNoBuildAlways	{ "--no-build-always", 0 };
		Option optPrintDesc		{ "--print-desc", 0 };
		Option optSUCF			{ "--enable-sucf", 0 };
		Option optPrintInHex	{ "--hex", 0 };
		Option optPrintCapital	{ "--HEX", 0 };
		Option optPrintZero		{ "--print-zero", 0 };
		Option optSpecConstant	{ "-specid", 1 };
		container_push_back_more(options, { optWorkGroupX, optWorkGroupY, optWorkGroupZ });
		container_push_back_more(options, { optLocalSizeX, optLocalSizeY, optLocalSizeZ });
		container_push_back_more(options, { optCheckpointMax, optPrintColCnt, optAddressMode });
		container_push_back_more(options, { optPrintParams, optPrintDesc, optPrintZero, optSpecConstant });

		resultParams.flags.printDesc = (consumeOptions(optPrintDesc, options, args, sink) > 0) ? 1 : 0;
		resultParams.flags.printParams = (consumeOptions(optPrintParams, options, args, sink) > 0) ? 1 : 0;
		resultParams.flags.enableSUCF = (consumeOptions(optSUCF, options, args, sink)) > 0 ? 1 : 0u;
		resultParams.flags.noPrintResult = (consumeOptions(optNoPrintRes, options, args, sink) > 0) ? 1 : 0;
		resultParams.flags.noBuildAlways = (consumeOptions(optNoBuildAlways, options, args, sink) > 0) ? 1 : 0;
		resultParams.flags.printZero = (consumeOptions(optPrintZero, options, args, sink) > 0) ? 1 : 0;
		{
			const uint32_t printInHex = (consumeOptions(optPrintInHex, options, args, sink) > 0) ? 1 : 0;
			const uint32_t printCapital = (consumeOptions(optPrintCapital, options, args, sink) > 0) ? 1 : 0;
			resultParams.flags.printInHex = ((printInHex | printCapital) != 0u) ? 1 : 0;
			resultParams.flags.printCapital = (printCapital != 0u) ? 1 : 0;
		}

		// localSize, workGroup, checkpointMax, printColCount, printRowCount
		if (true)
		{
			add_ptr<uint32_t> const sizeAccess[]
			{
				&resultParams.localSize.x(),
				&resultParams.localSize.y(),
				&resultParams.localSize.z(),
				&resultParams.workGroup.x(),
				&resultParams.workGroup.y(),
				&resultParams.workGroup.z(),
				&resultParams.printColCount,
				&resultParams.printRowCount,
				&resultParams.checkpointMax
			};
			add_cptr<Option> const sizeOptions[ARRAY_LENGTH(sizeAccess)]
			{
				&optLocalSizeX, &optLocalSizeY, &optLocalSizeZ,
				&optWorkGroupX, &optWorkGroupY, &optWorkGroupZ,
				&optPrintColCnt, &optPrintRowCnt, &optCheckpointMax
			};
			for (uint32_t i = 0; i < ARRAY_LENGTH_CAST(sizeOptions, uint32_t); ++i)
			{
				if (consumeOptions(*sizeOptions[i], options, args, sink) > 0)
				{
					int32_t k = fromText(sink.back(), make_signed(*sizeAccess[i]), status);
					if (status && (k > 0))
					{
						*sizeAccess[i] = make_unsigned(k);
					}
					else if (!status)
					{
						appendMessage(*sizeOptions[i], sink.back(), *sizeAccess[i]);
					}
					else
					{
						errorMessage << "{WARNING] \'" << sizeOptions[i]->name << "\' must be greater than 0,"
							<< " default \'" << *sizeAccess[i] << "\' was used." << std::endl;
					}
				}
			}
		}

		if (const int specConstantCount = consumeOptions(optSpecConstant, options, args, sink);
			specConstantCount > 0)
		{
			for (int k = 0; k < specConstantCount; ++k)
			{
				UVec2 specConstantDef;
				bool specConstantsStatus = false;
				std::array<bool, 2> specConstantsStatuses;
				add_cref<std::string> specText(sink.at(sink.size() - make_unsigned(specConstantCount) + make_unsigned(k)));
				specConstantDef = UVec2::fromText(specText, specConstantDef, specConstantsStatuses, &specConstantsStatus);
				if (specConstantsStatus)
				{
					resultParams.specConstants.push_back(specConstantDef);
				}
				else
				{
					errorMessage << "[WARNING] Unable to parse \'" << optSpecConstant.name
						<< " from \'" << specText << "\' - ignored." << std::endl;
				}
			}
		}

		// addressingMode
		if (true)
		{
			if (consumeOptions(optAddressMode, options, args, sink) > 0)
			{
				/*
				* TODO: temporary disabled
				if (strcasecmp(sink.back().c_str(), Params::addressingModeToString(AddresingMode::absolut)) == 0)
					resultParams.addressingMode = AddresingMode::absolut;
				else if (strcasecmp(sink.back().c_str(), Params::addressingModeToString(AddresingMode::local)) == 0)
					resultParams.addressingMode = AddresingMode::local;
				else
				{
					appendMessage(optAddressMode, sink.back(), std::string(addressingModeToString(resultParams.addressingMode)));
				}
				*/
			}
		}
	} // End of regular options

	const uint32_t invocationCount = ROUNDUP(resultParams.localSize.prod(), resultParams.subgroupSize);
	resultParams.inputIvalues.resize(invocationCount, 0u);
	resultParams.inputJvalues.resize(invocationCount, 0u);
	resultParams.inputTypes.resize(invocationCount, ((uint32_t(Float32) << 16) | Float32));

	enum class OptionKinds { Single, Const, Random };
	auto parseOtherOptions = [&](const OptionKinds optionKind)
	{
		char name[20u] {};
		const Option optInputValue {&name[0], 1u};

		typedef std::tuple<InputTypes, char> t2;
		typedef std::tuple<u32vec_ref, uint32_t, char> t1;
		for (add_cref<t1> it1 : {
									t1(resultParams.inputIvalues, 0u, 'i'),
									t1(resultParams.inputIvalues, 0u, 'I'),
									t1(resultParams.inputJvalues, 1u, 'j'),
									t1(resultParams.inputJvalues, 1u, 'J')
								})
		for (add_cref<t2> it2 : {
									t2(Int32, 'i'), t2(Int32, 'I'),
									t2(Uint32, 'u'), t2(Uint32, 'U'),
									t2(Float32, 'f'), t2(Float32, 'F'),
								})
		{
			std::fill(std::begin(name), std::end(name), '\0');
			name[0u] = '-';
			name[1u] = std::get<2>(it1);
			name[2u] = std::get<1>(it2);
			const std::string_view viewName(name);
			const InputTypes inputType = std::get<0>(it2);

			for (auto arg = args.begin(); arg != args.end();)
			{
				const std::string_view viewArg(*arg);
				if (viewArg.find(viewName) != 0)
				{
					++arg; continue;
				}

				const std::string tail(viewArg.substr(3u).data());
#if SYSTEM_OS_WINDOWS == 1
				strncat_s(name, tail.c_str(), (ARRAY_LENGTH(name) - 4u));
#else
				std::strncat(name, tail.c_str(), (ARRAY_LENGTH(name) - 4u));
#endif
				bool invStatus = false;
				const uint32_t inv = fromText(tail, INVALID_UINT32, invStatus);

				if (optionKind == OptionKinds::Random && tail == "-random")
				{
					arg = args.erase(arg);
					if (arg != args.end())
					{
						std::array<bool, 2> stats;
						add_cref<std::string> input = *arg;
						auto all_of_true = [](add_cref<bool> p) { return p; };

						if (inputType == Int32)
						{
							const IVec2 def(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
							const auto k = IVec2::fromText(input, def, stats);
							if ((k.x() == k.y()) || !std::all_of(stats.begin(), stats.end(), all_of_true))
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k.x(), k.y());
						}
						if (inputType == Uint32)
						{
							const UVec2 def(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
							const auto k = UVec2::fromText(input, def, stats);
							if ((k.x() == k.y()) || !std::all_of(stats.begin(), stats.end(), all_of_true))
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k.x(), k.y());
						}
						if (inputType == Float32)
						{
							const Vec2 def(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
							const auto k = Vec2::fromText(input, def, stats);
							if ((k.x() == k.y()) || !std::all_of(stats.begin(), stats.end(), all_of_true))
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k.x(), k.y());
						}
						arg = args.erase(arg);
					}
				}
				else if (optionKind == OptionKinds::Const && tail == "-const")
				{
					arg = args.erase(arg);
					if (arg != args.end())
					{
						bool constStatus = false;
						add_cref<std::string> input = *arg;

						if (inputType == Int32)
						{
							const int32_t def = 0;
							const auto k = fromText(input, def, constStatus);
							if (!constStatus)
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k, k);
						}
						if (inputType == Uint32)
						{
							const uint32_t def = 0u;
							const auto k = fromText(input, def, constStatus);
							if (!constStatus)
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k, k);
						}
						if (inputType == Float32)
						{
							const float def = 0.0f;
							const auto k = fromText(input, def, constStatus);
							if (!constStatus)
							{
								appendMessage(optInputValue, input, def);
							}
							resultParams.setInput(std::get<0>(it1), k, k);
						}
						arg = args.erase(arg);
					}
				}
				else if (invStatus && (optionKind == OptionKinds::Single) && (inv < invocationCount))
				{
					arg = args.erase(arg);
					if (arg != args.end())
					{
						add_cref<std::string> input = *arg;

						if (inputType == Int32)
						{
							const auto k = fromText(input, 0, invStatus);
							static_assert(std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(k)>>
								, int32_t>::value, "Not integral type");
							resultParams.setInput(k, inv, std::get<0>(it1));
							//std::cout << name << " = " << resultParams.printInputValue(inv, std::get<1>(it1)) << std::endl;
						}
						else if (inputType == Uint32)
						{
							const auto k = fromText(input, 0u, invStatus);
							static_assert(std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(k)>>
								, uint32_t>::value, "Not integral type");
							resultParams.setInput(k, inv, std::get<0>(it1));
							//std::cout << name << " = " << resultParams.printInputValue(inv, std::get<1>(it1)) << std::endl;
						}
						else if (inputType == Float32)
						{
							const auto k = fromText(input, 0.0f, invStatus);
							static_assert(std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(k)>>
								, float>::value, "Not floating type");
							resultParams.setInput(k, inv, std::get<0>(it1));
							//std::cout << name << " = " << resultParams.printInputValue(inv, std::get<1>(it1)) << std::endl;
						}

						if (invStatus)
						{
							resultParams.providedValueCount += 1u;
						}
						else
						{
							appendMessage(optInputValue, input, 0u);
						}

						arg = args.erase(arg);
					}
				}
				else
				{
					++arg;
				}
			}
		}
	};

	options.clear();
	sink.clear();

	parseOtherOptions(OptionKinds::Random);
	parseOtherOptions(OptionKinds::Const);
	parseOtherOptions(OptionKinds::Single);

	errorMessage.flush();
	Params::Status paramsStatus = resultParams.verify(device, errorMessage)
									? hasErrors
										? Params::Status::Error
										: hasWarnings
											? Params::Status::Warning
											: Params::Status::OK
									: Params::Status::Error;
	if (!args.empty())
	{
		paramsStatus = Params::Status::Error;
		errorMessage << "[ERROR] Unknown parameter: " << args.at(0) << std::endl;
	}

	return { paramsStatus, resultParams, errorMessage.str() };
}

void printOutputBuffer (ostream_ref log, add_cref<decltype(Params::flags)> flags,
						u32vec_cref types, u32vec_cref data,
						const uint32_t colCount = 4u, const uint32_t rowCount = 0u,
						const uint32_t count = 0u, const bool horizontal = false)
{
	UNREF(count);
	UNREF(horizontal);
	ASSERTION(horizontal == false);
	ASSERTION((rowCount > 0u) && (colCount > 0u));
	const uint32_t		itemCount	= data_count(data);
	const uint32_t		groupSize	= rowCount * colCount;
	const uint32_t		groupCount	= MULTIPLERUP(itemCount, groupSize);
	const int32_t		itemWidth	= static_cast<int32_t>(std::log10(float(itemCount))) + 1;
	const auto			calcWidth	= [&]() -> uint32_t {
		std::ostringstream os;
		const uint32_t	ii			= 0xFFFFFFFF;
		const float		pi			= std::atan(1.0f) * 4.0f;
		SIDE_EFFECT((flags.printInHex != 0) ? (os << std::hexfloat << pi) : (os << pi));
		const uint32_t	floatWidth	= uint32_t(os.str().length());
		os.str(std::string());
		SIDE_EFFECT((flags.printInHex != 0) ? (os << std::showbase << std::hex << ii) : (os << ii));
		const uint32_t	intWidth	= uint32_t(os.str().length());
		return std::max(floatWidth, intWidth);
	};
	const uint32_t		valueWidth	= calcWidth() + 2u;
	const std::string	blankValue	((valueWidth + 1), ' ');

	for (uint32_t i = 0u, g = 0u; g < groupCount; ++g)
	{
		const uint32_t gg = g * groupSize;
		const uint32_t n = (itemCount - gg) % rowCount;
		const uint32_t ccc = (itemCount - gg) / rowCount;

		for (uint32_t r = 0u; r < rowCount; ++r)
		{
			const uint32_t rr = r + gg;
			const uint32_t cc = ((itemCount - (g * groupSize)) < groupSize)
								? (ccc + ((r < n) ? 1u : 0u))
								: colCount;
			if (cc == 0u) break;

			for (uint32_t c = 0u; c < cc; ++c, ++i)
			{
				const uint32_t ii = rr + (c * rowCount);
				log << "[" << std::setw(itemWidth) << std::right << ii << "] ";
				const Params::InputTypes type = static_cast<Params::InputTypes>(std::min(
					(types.at(ii) & 0xFFFF), static_cast<uint32_t>(Params::InputTypes::Float32)));
				const uint32_t value = data.at(ii);
				if (value == 0u && (flags.printZero == 0))
					log << blankValue;
				else
				{
					log << std::left << std::setw(make_signed(valueWidth));
					if (flags.printCapital != 0)
						log << std::uppercase;
					if (type == Params::InputTypes::Float32)
					{
						union U {
							U(uint32_t u_) : u(u_) {}
							uint32_t	u;
							float		f;
						} x(value);
						if (flags.printInHex != 0)
							log << std::hexfloat << x.f << std::defaultfloat;
						else log << x.f;
					}
					else if (type == Params::InputTypes::Int32)
					{
						if (flags.printInHex != 0)
							log << std::showbase << std::hex << make_signed(value) << std::dec << std::noshowbase;
						else log << make_signed(value);
					}
					else // (type == Params::InputTypes::Uint32)
					{
						if (flags.printInHex != 0)
							log << std::showbase << std::hex << (value) << std::dec << std::noshowbase;
						else log << (value);
					}
					if (flags.printCapital != 0) log << std::nouppercase;
					log << std::setw(0);
					log << ' ';
				}
			}
			log << std::endl;
		}
	}
	log << std::setw(0) << std::right << std::endl;
}

void printInputParamsFromShader (ostream_ref log, add_cref<Params> params, add_cref<LayoutManager> lm, uint32_t bindingParams)
{
	const uint32_t bindingParamsLength = lm.getBindingElementCount(bindingParams);
	std::vector<uint32_t> p(bindingParamsLength);
	lm.readBinding(bindingParams, p);
	const uint32_t inputIlength = p.at(11);
	log << "gl_NumWorkGroups:             " << UVec3(p.at(0), p.at(1), p.at(2)) << std::endl
		<< "gl_WorkGroupSize:             " << UVec3(p.at(3), p.at(4), p.at(5)) << std::endl
		<< "max(gl_LocalInvocationIndex): " << p.at(6) << std::endl
		<< "gl_NumSubgroups:              " << p.at(7) << std::endl
		<< "gl_SubgroupSize:              " << p.at(8) << std::endl
		<< "addressingMode:               " << Params::addressingModeToString(Params::AddresingMode(p.at(9))) << std::endl
		<< "checkpointMax:                " << p.at(10) << std::endl
		<< "inputI.i.length():            " << inputIlength << std::endl
		<< "outputP.p.length():           " << p.at(12) << std::endl;
	const uint32_t controlIndices[]
	{
		params.controlIndex0,
		params.controlIndex1,
		params.controlIndex2,
		params.controlIndex3,
		params.controlIndex4
	};
	for (const auto& q : { std::make_pair(0u, 'I'), std::make_pair(1u, 'J') })
	{
		bool atLeastOne = false;
		log << "inputValues " << q.second << ": ";
		for (uint32_t i = 0; i < ARRAY_LENGTH(controlIndices); ++i)
		{
			if (controlIndices[i] >= inputIlength) continue;
			if (atLeastOne) std::cout << ", ";
			log << '[' << controlIndices[i] << "]:"
				<< params.printInputValue(p.at(i + (q.first + 1u) * 16u), controlIndices[i], q.first);
			atLeastOne = true;
		}
		std::cout << std::endl;
	}
}

TriLogicInt runIntComputeSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params);

void printDeviceLimits (ZDevice device, add_ref<std::ostream> log)
{
	const auto [maxComputeWorkGroupCount, maxComputeWorkGroupSize,
				maxComputeWorkGroupInvocations, gl_SubgroupSize, maxComputeSharedMemorySize]
			= getDeviceLimits(device);

	log << "maxComputeSharedMemorySize:     " << maxComputeSharedMemorySize << std::endl;
	log << "maxComputeWorkGroupCount:       " << maxComputeWorkGroupCount << std::endl;
	log << "maxComputeWorkGroupSize:        " << maxComputeWorkGroupSize << std::endl;
	log << "maxComputeWorkGroupInvocations: " << maxComputeWorkGroupInvocations << std::endl;
	log << "gl_SubgroupSize:                " << gl_SubgroupSize << std::endl;
}

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(record);
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	auto hasCmdLineSUCF = [&]() -> bool
	{
		Option optSUCF { "--enable-sucf", 0 };
		const std::vector<Option> options { optSUCF };
		strings sink, cmdLineParamsCopy(cmdLineParams);
		return (consumeOptions(optSUCF, options, cmdLineParamsCopy, sink) > 0);
	};
	const bool sucfEnabled = hasCmdLineSUCF();
	VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR sucfFeatures = makeVkStruct();
	VkPhysicalDeviceFeatures2 resultFeatures = makeVkStruct(sucfEnabled ? &sucfFeatures : nullptr);
	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		if (sucfEnabled)
		{
			extensions.emplace_back(VK_KHR_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_EXTENSION_NAME);
		}
		vkGetPhysicalDeviceFeatures2(*physicalDevice, &resultFeatures);
		return resultFeatures;
	};
	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);
	const auto [status, params, errorMessage] = Params::parseCommandLine(ctx.device, cmdLineParams);
	if (status == Params::Status::Limits)
	{
		printDeviceLimits(ctx.device, std::cout);
		return 0;
	}
	computePipelineVerifyLimits(ctx.device, params.localSize, true);
	if (status == Params::Status::Help)
	{
		params.usage(ctx.device, std::cout, record.assets);
		return 0;
	}
	if (params.flags.printParams != 0)
	{
		if (!errorMessage.empty())
			std::cout << errorMessage;
		params.print(std::cout);
	}
	std::cout << errorMessage;
	if (status == Params::Status::Error)
	{
		return (1);
	}

	return runIntComputeSingleThread(ctx, record.assets, params);
}

ZShaderModule createShader (ZDevice device, add_cref<std::string> assets, bool buildAlways)
{
	const GlobalAppFlags	flags(getGlobalAppFlags());
	ProgramCollection		programs(device, assets);

	programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "main.compute", {"."}, "main_entry");
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly, buildAlways);

	return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt runIntComputeSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params)
{
	const VkShaderStageFlagBits	stage			(VK_SHADER_STAGE_COMPUTE_BIT);
	const VkDescriptorType		descType		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ZShaderModule				computeShader	= createShader(ctx.device, assets, (params.flags.noBuildAlways ? false : true));
	struct PushConstant
	{
		Params::AddresingMode	addressingMode;
		uint32_t	checkpointMax;
		uint32_t	i0, i1, i2, i3, i4;
	} const						pc				{	params.addressingMode, params.checkpointMax,
													params.controlIndex0, params.controlIndex1,
													params.controlIndex2, params.controlIndex3, params.controlIndex4 };
	LayoutManager				lm				(ctx.device);
	const uint32_t				bindingParams	= lm.addBinding<std::vector<uint32_t>>(descType, params.paramsBufferLength, stage);
	const uint32_t				bindingTypes	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputTypes), stage);
	const uint32_t				bindingIinput	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputIvalues), stage);
	const uint32_t				bindingJinput	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputJvalues), stage);
	const uint32_t				bindingResult	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputTypes), stage);
	ZPipelineLayout				pipelineLayout	= lm.createPipelineLayout<PushConstant>(lm.createDescriptorSetLayout(), stage);

	ZSpecializationInfo	specInfo;
	specInfo.addEntries(ZSpecEntry<int32_t>(make_signed(params.localSize.x()), std::numeric_limits<int32_t>::max() - 3),	// 2147483644
						ZSpecEntry<int32_t>(make_signed(params.localSize.y()), std::numeric_limits<int32_t>::max() - 2),	// 2147483645
						ZSpecEntry<int32_t>(make_signed(params.localSize.z()), std::numeric_limits<int32_t>::max() - 1));	// 2147483646
	for (add_cref<UVec2> entry : params.specConstants)
	{
		specInfo.addEntry<uint32_t>(entry.y(),	 entry.	x());
	}

	ZPipeline					pipeline		= createComputePipeline(pipelineLayout, computeShader, specInfo);
	ZCommandPool				commandPool		= ctx.createComputeCommandPool();

	lm.fillBinding(bindingParams, 0u);
	lm.writeBinding(bindingTypes, params.inputTypes);
	lm.writeBinding(bindingIinput, params.inputIvalues);
	lm.writeBinding(bindingJinput, params.inputJvalues);
	lm.fillBinding(bindingResult, 0u);

	params.inputJvalues.clear();
	
	// RAII
	{
		OneShotCommandBuffer shot(commandPool);
		ZCommandBuffer shotCmd = shot.commandBuffer;
		commandBufferPushConstants<PushConstant>(shotCmd, pipelineLayout, pc);
		commandBufferBindPipeline(shotCmd, pipeline);
		commandBufferDispatch(shotCmd, UVec3(params.workGroup));
	}

	if (params.flags.noPrintResult == 0u)
	{
		lm.readBinding(bindingTypes, params.inputTypes);
		lm.readBinding(bindingResult, params.inputIvalues);

		printOutputBuffer(std::cout, params.flags,
							params.inputTypes, params.inputIvalues,
							params.printColCount, params.printRowCount);
	}

	if (params.flags.printParams != 0u)
	{
		printInputParamsFromShader(std::cout, params, lm, bindingParams);
	}

	return 0;
}

} // unnamed namespace

template<> struct TestRecorder<INT_COMPUTE>
{
	static bool record (TestRecord&);
};
bool TestRecorder<INT_COMPUTE>::record (TestRecord& record)
{
	record.name = "int_compute";
	record.call = &prepareTests;
	return true;
}
