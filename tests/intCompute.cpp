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
#include <stdio.h>

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
		OK, Help, Limits, Error
	};
	enum InputTypes
	{
		Unknown,
		Int32,
		Float32
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
	std::vector<uint32_t>	inputIvalues;
	std::vector<uint32_t>	inputJvalues;
	std::vector<uint32_t>	inputTypes;
	struct {
		uint32_t printParams	: 1;	// print params from cmdLine and continue
		uint32_t printDesc		: 1;	// print description and exits
		uint32_t printInHex		: 1;	// print numbers in hexadecimal format
		uint32_t printCapital	: 1;	// print capital characters
		uint32_t printZero		: 1;	// print zero in result
		uint32_t noPrintResult	: 1;	// don't print result
		uint32_t enableSUCF		: 1;	// enable subgroup_uniform_control_flow extension
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
	};
	void		print (add_ref<std::ostream> log) const;
	void		usage (ZDevice device, add_ref<std::ostream> log) const;
	bool		verify (ZDevice device, add_ref<std::ostream> log) const;
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
	template<class X> void setInput (const X& val, uint32_t at, u32vec_ref values);
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
	switch ((InputTypes)((uint8_t const*)&inputTypes.at(streamThrough.at))[streamThrough.sto])
	{
	case Int32:		str << std::setw(8) << inputValue << std::setw(0); break;
	case Float32:	str << std::setw(8) << UVec1(inputValue).bitcast<Vec1>().x() << std::setw(0); break;
	default:		str << "<unk>" << std::setw(3) << inputValue << std::setw(0); break;
	}
}
template<> void Params::setInput<uint32_t> (const uint32_t& val, uint32_t at, u32vec_ref values)
{
	// looks like stupid but only in test purpose
	uint32_t typeIdx = 0u;
	if (&values == &inputIvalues)
		typeIdx = 0u;
	else if (&values == &inputJvalues)
		typeIdx = 1u;
	else { ASSERTION(0); }
	((uint8_t*)(&inputTypes.at(at)))[typeIdx] = Int32;
	values.at(at) = val;
}
template<> void Params::setInput<float> (const float& val, uint32_t at, u32vec_ref values)
{
	// looks like stupid but only in test purpose
	uint32_t typeIdx = 0u;
	if (&values == &inputIvalues)
		typeIdx = 0u;
	else if (&values == &inputJvalues)
		typeIdx = 1u;
	else { ASSERTION(0); }
	((uint8_t*)(&inputTypes.at(at)))[typeIdx] = Float32;
	values.at(at) = Vec1(val).bitcast<UVec1>().x();
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
void Params::usage (ZDevice device, add_ref<std::ostream> log) const
{
	const Params def(getSystemSubgroupSize(device));
	log << "Usage:\n"
		<< "  [-h]                            print usage and exit\n"
		<< "  [--help]                        print usage and exit\n"
		<< "  [--print-desc]                  print description and execute\n"
		<< "  [--print-params]                print params and execute\n"
		<< "  {--print-zero]                  print zero in result, default false\n"
		<< "  [--no-print-result]             prevent from printing result\n"
		<< "  [--enable-sucf]                 enable VK_KHR_shader_subgroup_uniform_control_flow extension\n"
		<< "  [-limits]                       print device limits and exit\n"
		<< "  [-hex]                          print result in hexadecimal format" << std::endl
		<< "  [-HEX]                          print result using capital characters" << std::endl
		<< "  [-wx <uint>]                    work group count X, default " << def.workGroup.x() << std::endl
		<< "  [-wy <uint>]                    work group count Y, default " << def.workGroup.y() << std::endl
		<< "  [-wz <uint>]                    work group count Z, default " << def.workGroup.z() << std::endl
		<< "  [-lx <uint>]                    local size X, default " << def.localSize.x() << std::endl
		<< "  [-ly <uint>]                    local size Y, default " << def.localSize.y() << std::endl
		<< "  [-lz <uint>]                    local size Z, default " << def.localSize.z() << std::endl
		<< "  [-cm <uint>]                    checkpoint count max, default " << def.checkpointMax << std::endl
		<< "  [-cols <uint>                   column count used to printing output, default " << def.printColCount << std::endl
		<< "  [-rows <uint>                   row count used to printing output, default " << def.printRowCount << std::endl
		<< "  [-am <string>]                  addressing mode, default " << addressingModeToString(def.addressingMode) << std::endl
		<< "                                   * " << addressingModeToString(AddresingMode::local) << ": gl_LocalInvocationID\n"
		<< "                                   * " << addressingModeToString(AddresingMode::absolut)
												   << ": gl_SubgroupID * gl_SubgroupSize + gl_SubgroupInvocationID\n"
		<< "  [[-(i|I)Nth <int>]...]          set int32 value for N'th invocation in buffer 0\n"
		<< "  [[-(j|J)Nth <int>]...]          set int32 value for N'th invocation in buffer 1\n"
		<< "  [[-((i|I)(f|F))Nth <float>]...] set float32 value for N'th invocation in buffer 0\n"
		<< "  [[-((j|J)(f|F))Nth <float>]...] set float32 value for N'th invocation in buffer 1"
		<< std::endl;
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
std::tuple<Params::Status, Params, std::string> Params::parseCommandLine (ZDevice device, add_cref<strings> cmdLineParams)
{
	bool status;
	strings sink;
	strings args(cmdLineParams);
	std::vector<Option> options;
	std::stringstream errorMessage;
	Params resultParams(getSystemSubgroupSize(device));

	Option optHelpShort		{ "-h", 0 };
	Option optHelpLong		{ "--help", 0 };
	Option optLimits		{ "-limits", 0 };
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

	auto appendMessage = [&](add_cref<Option> opt, add_cref<std::string> val, const auto& def, bool error = false) -> void
	{
		if (error)
			errorMessage << "[ERROR] ";
		else errorMessage << "[WARNING] ";
		errorMessage << "Unable to parse \'" << opt.name << " from \'" << val << "\' "
					 << ", default value \'" << def << "\' was used" << std::endl;
	};

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
	Option optPrintDesc		{ "--print-desc", 0 };
	Option optSUCF			{ "--enable-sucf", 0 };
	Option optPrintInHex	{ "-hex", 0 };
	Option optPrintCapital	{ "-HEX", 0 };
	Option optPrintZero		{ "--print-zero", 0 };
	container_push_back_more(options, { optWorkGroupX, optWorkGroupY, optWorkGroupZ });
	container_push_back_more(options, { optLocalSizeX, optLocalSizeY, optLocalSizeZ });
	container_push_back_more(options, { optCheckpointMax, optPrintColCnt, optAddressMode });
	container_push_back_more(options, { optPrintParams, optPrintDesc, optPrintZero });

	resultParams.flags.printDesc = (consumeOptions(optPrintDesc, options, args, sink) > 0) ? 1 : 0;
	resultParams.flags.printParams = (consumeOptions(optPrintParams, options, args, sink) > 0) ? 1 : 0;
	resultParams.flags.enableSUCF = (consumeOptions(optSUCF, options, args, sink)) > 0 ? 1 : 0u;
	resultParams.flags.noPrintResult = (consumeOptions(optNoPrintRes, options, args, sink) > 0) ? 1 : 0;
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
		add_ptr<uint32_t>	const sizeAccess[]	{ &resultParams.localSize.x(),
												  &resultParams.localSize.y(),
												  &resultParams.localSize.z(),
												  &resultParams.workGroup.x(),
												  &resultParams.workGroup.y(),
												  &resultParams.workGroup.z(),
												  &resultParams.printColCount,
												  &resultParams.printRowCount,
												  &resultParams.checkpointMax };
		add_cptr<Option>	const sizeOptions[ARRAY_LENGTH(sizeAccess)]
												{ &optLocalSizeX, &optLocalSizeY, &optLocalSizeZ,
												  &optWorkGroupX, &optWorkGroupY, &optWorkGroupZ,
												  &optPrintColCnt, &optPrintRowCnt, &optCheckpointMax };
		for (uint32_t i = 0; i < ARRAY_LENGTH_CAST(sizeOptions, uint32_t); ++i)
		{
			if (consumeOptions(*sizeOptions[i], options, args, sink) > 0)
			{
				uint32_t k = fromText(sink.back(), *sizeAccess[i], status);
				if (status && (k != 0u))
				{
					*sizeAccess[i] = k;
				}
				else if (!status)
				{
					appendMessage(*sizeOptions[i], sink.back(), *sizeAccess[i]);
				}
				else
				{
					errorMessage << "{WARNING] \'" << sizeOptions[i]->name << "\' must not be 0,"
								 << " default \'" << *sizeAccess[i] << "\' was used." << std::endl;
				}
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

	// inputValues
	if (true)
	{
		const uint32_t invocationCount = ROUNDUP(resultParams.localSize.prod(), resultParams.subgroupSize);
		resultParams.inputIvalues.resize(invocationCount, 0u);
		resultParams.inputJvalues.resize(invocationCount, 0u);
		resultParams.inputTypes.resize(invocationCount, Unknown);
		char name[20u];
		options.push_back(Option{&name[0], 1u});
		add_ref<Option> optInputValue = options.back();

		typedef std::tuple<u32vec_ref, uint32_t, char, char> t1;
		for (add_cref<t1> it1 : { t1(resultParams.inputIvalues, 0u, 'i', 'I'), t1(resultParams.inputJvalues, 1u, 'j', 'J') })
		{
			typedef std::tuple<InputTypes, uint32_t, uint32_t, char, char> t2;
			for (const char inputName : { std::get<2>(it1), std::get<3>(it1) })
			for (uint32_t inv = 0; inv < invocationCount; ++inv)
			for (add_cref<t2> it2 : { t2(Int32, 1u, 2u, 'x', 'x'), t2(Float32, 2u, 3u, 'f', 'F') })
			for (uint32_t repeat = 0u; repeat < std::get<1>(it2); ++repeat)
			{
				const char floatSwitch = (0 == repeat) ? std::get<3>(it2) : std::get<4>(it2);
				const InputTypes inputType = std::get<0>(it2);

				std::fill(std::begin(name), std::end(name), '\0');
				name[0u] = '-';
				name[1u] = inputName;
				name[2u] = floatSwitch;
				std::to_chars(&name[std::get<2>(it2)], ((&name[0]) + ARRAY_LENGTH(name) - std::get<2>(it2)), inv, 10);

				status = false;

				if (consumeOptions(optInputValue, options, args, sink) > 0)
				{
					if (inputType == Int32)
					{
						const auto k = fromText(sink.back(), 0u, status);
						static_assert(std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(k)>>
									  , uint32_t>::value, "Not integral type");
						resultParams.setInput(k, inv, std::get<0>(it1));
						//std::cout << name << " = " << resultParams.printInputValue(inv, std::get<1>(it1)) << std::endl;
					}
					else if (inputType == Float32)
					{
						const auto k = fromText(sink.back(), 0.0f, status);
						static_assert(std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(k)>>
									  , float>::value, "Not floating type");
						resultParams.setInput(k, inv, std::get<0>(it1));
						//std::cout << name << " = " << resultParams.printInputValue(inv, std::get<1>(it1)) << std::endl;
					}
					else ASSERTION(false);
					if (status)
					{
						resultParams.providedValueCount += 1u;
					}
					else
					{
						appendMessage(optInputValue, sink.back(), 0u);
					}
				}
			}
		}
	}

	errorMessage.flush();
	Params::Status paramsStatus = resultParams.verify(device, errorMessage)
									? Params::Status::OK
									: Params::Status::Error;
	if (!args.empty())
	{
		paramsStatus = Params::Status::Error;
		errorMessage << "[ERROR] Unknown parameter: " << args.at(0) << std::endl;
	}

	return { paramsStatus, resultParams, errorMessage.str() };
}

void printOutputBuffer (ostream_ref log, add_cref<Params> params, u32vec_cref data,
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
	const auto			calcWidth	= [&]() -> int {
		std::ostringstream os;
		const uint32_t	ii			= 0xFFFFFFFF;
		const float		pi			= std::atan(1.0f) * 4.0f;
		SIDE_EFFECT((params.flags.printInHex != 0) ? (os << std::hexfloat << pi) : (os << pi));
		const int		floatWidth	= int(os.str().length());
		os.str(std::string());
		SIDE_EFFECT((params.flags.printInHex != 0) ? (os << std::showbase << std::hex << ii) : (os << ii));
		const int		intWidth	= int(os.str().length());
		return std::max(floatWidth, intWidth);
	};
	const int			valueWidth	= calcWidth();
	const std::string	blankValue	(make_unsigned(valueWidth + 1), ' ');

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
				const uint32_t type = params.inputTypes.at(ii);
				const uint32_t value = data.at(ii);
				if (value == 0u && (params.flags.printZero == 0))
					log << blankValue;
				else
				{
					log << std::left << std::setw(valueWidth);
					if (params.flags.printCapital != 0)
						log << std::uppercase;
					if (type == 2)
					{
						union U {
							U(uint32_t u_) : u(u_) {}
							uint32_t	u;
							float		f;
						} x(value);
						if (params.flags.printInHex != 0)
							log << std::hexfloat << x.f << std::defaultfloat;
						else log << x.f;
					}
					else
					{
						if (params.flags.printInHex != 0)
							log << std::showbase << std::hex << value << std::dec << std::noshowbase;
						else log << value;
					}
					if (params.flags.printCapital != 0) log << std::nouppercase;
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

int runIntComputeSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params);

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

int prepareTests (const TestRecord& record, const strings& cmdLineParams)
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
	if (status == Params::Status::Help)
	{
		params.usage(ctx.device, std::cout);
		return 0;
	}
	if (params.flags.printParams != 0)
	{
		if (!errorMessage.empty())
			std::cout << errorMessage;
		params.print(std::cout);
	}
	if (status == Params::Status::Error)
	{
		std::cout << errorMessage;
		return (1);
	}

	return runIntComputeSingleThread(ctx, record.assets, params);
}

ZShaderModule createShader (ZDevice device, add_cref<std::string> assets, VkShaderStageFlagBits stage)
{
	const GlobalAppFlags	flags(getGlobalAppFlags());
	ProgramCollection		programs(device, assets);

	programs.addFromFile(stage, "main.compute", {"."}, "main_entry");
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, false, true);

	return programs.getShader(stage);
}

int runIntComputeSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params)
{
	const VkShaderStageFlagBits	stage			(VK_SHADER_STAGE_COMPUTE_BIT);
	const VkDescriptorType		descType		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ZShaderModule				computeShader	= createShader(ctx.device, assets, stage);
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
	const uint32_t				bindingTypes	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputIvalues), stage);
	const uint32_t				bindingIinput	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputIvalues), stage);
	const uint32_t				bindingJinput	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputJvalues), stage);
	const uint32_t				bindingResult	= lm.addBinding<std::vector<uint32_t>>(descType, data_count(params.inputJvalues), stage);
	ZPipelineLayout				pipelineLayout	= lm.createPipelineLayout<PushConstant>(lm.createDescriptorSetLayout(), stage);
	ZPipeline					pipeline		= createComputePipeline(pipelineLayout, computeShader, params.localSize);
	ZCommandPool				commandPool		= ctx.createComputeCommandPool();

	lm.fillBinding(bindingParams, 0u);
	lm.writeBinding(bindingTypes, params.inputTypes);
	lm.writeBinding(bindingIinput, params.inputIvalues);
	lm.writeBinding(bindingJinput, params.inputJvalues);
	lm.fillBinding(bindingResult, 0u);

	// RAII
	{
		OneShotCommandBuffer shot(commandPool);
		ZCommandBuffer shotCmd = shot.commandBuffer;
		commandBufferPushConstants<PushConstant>(shotCmd, pipelineLayout, pc);
		commandBufferBindPipeline(shotCmd, pipeline);
		commandBufferDispatch(shotCmd, params.workGroup);
	}

	const uint32_t	resultLength	= lm.getBindingElementCount(bindingResult);
	u32vec			resultData		(resultLength);
	lm.readBinding(bindingResult, resultData);

	if (params.flags.noPrintResult == 0u)
	{
		printOutputBuffer(std::cout, params, resultData, params.printColCount, params.printRowCount);
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
