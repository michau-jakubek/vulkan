#include <iostream>
#include <cstring>
#include <sstream>
#include <string_view>

#include "main.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfContext.hpp"
#include "vtfFilesystem.hpp"
#include "allTests.hpp"

using namespace vtf;

std::vector<TestRecord> AllTestRecords;

static void printUsage(std::ostream& str);
static std::string constructCompleteCommand(const char* appPath);

int main(int argc, char* argv[])
{
	recordAllTests(AllTestRecords);

	if (argc < 2)
	{
		std::cout << "Pass available option as a parameter:" << std::endl;
		printUsage(std::cout);
		std::cout << "Available test list:" << std::endl;
		return 1;
	}

	strings allArgs(&argv[1], &argv[1]+argc-1);
	auto testNamePos = allArgs.end();

	{
		const auto testNames = getTestNames(AllTestRecords);
		for (uint32_t i = 0; testNamePos == allArgs.end() && i < static_cast<uint32_t>(testNames.size()); ++i)
		{
			testNamePos = std::find_if(allArgs.begin(), allArgs.end(),
								   [&](const std::string& s) {
				return std::strcmp(s.c_str(), testNames[i]) == 0;
			});
		}
	}

	strings appArgs;
	if (allArgs.end() != testNamePos)
		std::copy(allArgs.begin(), testNamePos, std::back_inserter(appArgs));
	else std::copy(allArgs.begin(), allArgs.end(), std::back_inserter(appArgs));

	bool status;
	strings sink;
	std::vector<Option>	options;

	Option help1	{ "-h", 0 };		options.push_back(help1);
	Option help2	{ "--help", 0 };	options.push_back(help2);
	Option testList	{ "-t", 0 };		options.push_back(testList);
	Option complCmd	{ "-c", 0 };		options.push_back(complCmd);
	Option devList	{ "-dl", 0 };		options.push_back(devList);
	Option curDev	{ "-d", 1 };		options.push_back(curDev);
	Option layer	{ "-l", 1 };		options.push_back(layer);
	Option layList	{ "-ll", 0 };		options.push_back(layList);
	Option btrace	{ "-bt", 0 };		options.push_back(btrace);

	Option vulkan	{ "-vulkan", 1 };	options.push_back(vulkan);
	Option spirv	{ "-spirv",  1 };	options.push_back(spirv);
	Option spvValid	{ "-spvvalid", 0 };	options.push_back(spvValid);
	Option verbose	{ "-verbose", 0 };	options.push_back(verbose);

	if (consumeOptions(help1, options, appArgs, sink) > 0
		|| consumeOptions(help2, options, appArgs, sink) > 0)
	{
		printUsage(std::cout);
		return 0;
	}

	if (consumeOptions(testList, options, appArgs, sink) > 0)
	{
		std::cout << "Available test list:" << std::endl;
		printAvailableTests(std::cout, AllTestRecords, "\t", true);
		return 0;
	}

	if (consumeOptions(complCmd, options, appArgs, sink) > 0)
	{
		std::cout << constructCompleteCommand(THIS_EXECUTABLE_NAME) << std::endl;
		return 0;
	}

	if (consumeOptions(devList, options, appArgs, sink) > 0)
	{
		ZInstance instance = createInstance("test", getAllocationCallbacks());
		printPhysicalDevices(*instance, std::cout);
		return 0;
	}

	if (consumeOptions(layList, options, appArgs, sink) > 0)
	{
		for (const auto& name : enumerateInstanceLayers())
			std::cout << name << std::endl;
		return 0;
	}

	if (consumeOptions(curDev, options, appArgs, sink) > 0)
	{
		VulkanContext::deviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}
	}

	if (consumeOptions(btrace, options, appArgs, sink) > 0)
	{
		backtraceEnabled(fromText(sink.back(), 0, status) != 0);
	}

	uint32_t	vulkanVer	= Version::make(1, 0);
	if (consumeOptions(vulkan, options, appArgs, sink) > 0)
	{
		vulkanVer = fromText(sink.back(), 10u, status);
		if (!status)
		{
			std::cout << "Unable to parse Vulkan version, default 1.0 will be used" << std::endl;
		}
		else
		{
			const Version version = Version::from10xMajorPlusMinor(vulkanVer);
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u, "Mojor Vulkan version must be from 1 to 9");
			vulkanVer = version;
		}
	}

	uint32_t	spirvVer	= Version::make(1, 0);
	if (consumeOptions(spirv, options, appArgs, sink) > 0)
	{
		spirvVer= fromText(sink.back(), 10u, status);
		if (!status)
		{
			std::cout << "Unable to parse SPIR-V version, default 1.0 will be used" << std::endl;
		}
		else
		{
			const Version version = Version::from10xMajorPlusMinor(spirvVer);
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u, "Mojor SPIR-V version must be from 1 to 9");
			spirvVer = version;
		}
	}

	bool	spirvValidate	= consumeOptions(spvValid, options, appArgs, sink) > 0;
	setAppVerboseFlag(consumeOptions(verbose, options, appArgs, sink) > 0);

	if (allArgs.end() == testNamePos)
	{
		std::cout << "Unable to find any test name in app parameters" << std::endl;
		printUsage(std::cout);
		return 1;
	}

	TestRecord testRecord;
	findTestByName(testRecord, AllTestRecords, testNamePos->c_str());
	testRecord.deviceIndex = VulkanContext::deviceIndex;
	consumeOptions(layer, options, appArgs, testRecord.layers);
	testRecord.assets			= (fs::path(ASSETS_PATH) / *testNamePos / "").generic_u8string().c_str();
	testRecord.vulkanVer		= vulkanVer;
	testRecord.spirvVer			= spirvVer;
	testRecord.spvValidation	= spirvValidate;

	strings testArgs;
	std::copy(std::next(testNamePos), allArgs.end(), std::back_inserter(testArgs));
	int result = (*testRecord.call)(testRecord, testArgs);

	std::cout << "The test \"" << *testNamePos << "\" " << (result == 0 ? "Passed" : "FAILED") << std::endl;
	return result;
}

void printUsage(std::ostream& str)
{
	str << "Usage: app [options] <test_name> [<test_param>,...]" << std::endl;
	str << "Options:" << std::endl;
	str << "  -h, --help:     prints this help and exits" << std::endl;
	str << "  -t:             prints available test names" << std::endl;
	str << "  -c:             builds auto-complete command" << std::endl;
	str << "  -dl:            prints available device list" <<std::endl;
	str << "  -d <id>:        picks device by id" <<std::endl;
	str << "  -ll:            prints available instance layer names" << std::endl;
	str << "  -l <layer> [-l <layer>]: enable layer(s)" << std::endl;
	str << "  -bt:            enable backtrace" << std::endl;
	str << "  -vulkan <ver>:  (major * 10 + minor), default is 10 aka vulkan1.0" << std::endl;
	str << "  -spirv <ver>:   (major * 10 + minor), default is 10 aka spirv1.0" << std::endl;
	str << "  -spvvalid:      enable SPIR-V assembly validation" << std::endl;
	str << "  -verbose:       enable application diagnostic messages" << std::endl;
	str << "Available tests are:" << std::endl;
	printAvailableTests(str, AllTestRecords, "\t", true);
}

std::string constructCompleteCommand(const char* appPath)
{
	const auto names = getTestNames(AllTestRecords);
	std::stringstream ss;
	ss << "complete -W ";
	ss << '\'';
	for (typename std::remove_const<decltype(names)>::type::size_type i = 0; i < names.size(); ++i)
	{
		if (i) ss << ' ';
		ss << names[i];
	}
	ss << '\'';
	ss << ' ' << appPath; // getRealPath(appPath, realpathStatus);
	return ss.str();
}

static bool globalDebugApp;

bool getGlobalAppDebug () { return globalDebugApp; }
void setGlobalAppDebug (bool enable) { globalDebugApp = enable; }
