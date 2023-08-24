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

static void printUsage (std::ostream& str);
static std::string constructCompleteCommand (const char* appPath);

int parseParams (int argc, char* argv[], add_ref<TestRecord> testRecord, add_ref<strings> testArgs, add_ref<bool> performTest)
{
	performTest = false;

	if (argc < 2)
	{
		std::cout << "Pass available option as a parameter:" << std::endl;
		printUsage(std::cout);
		std::cout << "Available test list:" << std::endl;
		return 1;
	}

	int consumeRes = (-1);
	strings allArgs(&argv[1], &argv[1] + argc - 1);
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

	Option help1{ "-h", 0 };					options.push_back(help1);
	Option help2{ "--help", 0 };				options.push_back(help2);
	Option testList{ "-t", 0 };					options.push_back(testList);
	Option complCmd{ "-c", 0 };					options.push_back(complCmd);
	Option devList{ "-dl", 0 };					options.push_back(devList);
	Option extList{ "-de", 1 };					options.push_back(extList);
	Option curDev{ "-d", 1 };					options.push_back(curDev);
	Option layer{ "-l", 1 };					options.push_back(layer);
	Option layList{ "-ll", 0 };					options.push_back(layList);
	Option optLayNoVuid{ "-l-no-vuid-undefined", 0 };	options.push_back(optLayNoVuid);
	Option optAssets{ "-assets", 1 };				options.push_back(optAssets);
	Option optTempDir{ "-tmp", 1 };             options.push_back(optTempDir);
	Option btrace{ "-bt", 0 };					options.push_back(btrace);
	Option optApi{ "-api", 1 };					options.push_back(optApi);
	Option vulkan{ "-vulkan", 1 };				options.push_back(vulkan);
	Option spirv{ "-spirv",  1 };				options.push_back(spirv);
	Option spvValid{ "-spvvalid", 0 };			options.push_back(spvValid);
	Option verbose{ "-verbose", 1 };			options.push_back(verbose);
	Option nowerror{ "-nowerror", 0 };			options.push_back(nowerror);
	Option dprintf{ "-dprintf", 0 };			options.push_back(dprintf);

	GlobalAppFlags globalAppFlags;

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
		globalAppFlags.physicalDeviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}
	}

	if (consumeOptions(verbose, options, appArgs, sink) > 0)
	{
		globalAppFlags.verbose = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse verbose level" << std::endl;
			return 1;
		}
	}

	if (consumeOptions(btrace, options, appArgs, sink) > 0)
	{
		backtraceEnabled(fromText(sink.back(), 0, status) != 0);
	}

	uint32_t	apiVer = Version::make(1, 1);
	consumeRes = consumeOptions(optApi, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optApi.name << "\" (Veulkan API version) option param" << std::endl;
		return 2;
	}
	else if (consumeRes > 0)
	{
		apiVer = fromText(sink.back(), 11u, status);
		if (!status)
		{
			std::cout << "Unable to parse Vulkan API version, default "
					  << globalAppFlags.apiVer.nmajor << '.' << globalAppFlags.apiVer.nminor
					  << " will be used" << std::endl;
		}
		else
		{
			const Version version = Version::from10xMajorPlusMinor(apiVer);
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u, "Mojor API version must be from 1 to 9");
			globalAppFlags.apiVer.update(version);
			apiVer = version;
		}
	}

	if (consumeOptions(extList, options, appArgs, sink) > 0)
	{
		const uint32_t deviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}

		consumeOptions(layer, options, appArgs, globalAppFlags.layers);

		ZInstance			instance = createInstance(THIS_EXECUTABLE_NAME
													  , getAllocationCallbacks()
													  , globalAppFlags.layers	// const strings&	desiredLayers
													  , {}						// const strings&	desiredExtension
													  , nullptr					// pMessenger
													  , nullptr					// pMessengerUserData
													  , nullptr					// pReport
													  , nullptr					// pReportUserData
													  , apiVer					// apiVersion
													  , false					// enableDebugPrintf
													  );
		ZPhysicalDevice		phys = selectPhysicalDevice(deviceIndex, instance, {/* required extensions */});
		add_cref<strings>	exts	= phys.getParamRef<strings>();
		uint32_t			entry	= 0;
		for (add_cref<strings::value_type> ext : exts)
		{
			if (entry) std::cout << '\n';
			std::cout << std::setw(3) << (entry++) << std::setw(0) << ": " << ext;
		}
		std::cout << std::endl;
		return 0;
	}

	uint32_t	vulkanVer = Version::make(1, 0);
	consumeRes = consumeOptions(vulkan, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << vulkan.name << "\" (vulkan compile target) option param" << std::endl;
		return 2;
	}
	else if (consumeRes > 0)
	{
		vulkanVer = fromText(sink.back(), 10u, status);
		if (!status)
		{
			std::cout << "Unable to parse Vulkan version, default "
					  << globalAppFlags.vulkanVer.nmajor << '.' << globalAppFlags.vulkanVer.nminor
					  << " will be used" << std::endl;
		}
		else
		{
			const Version version = Version::from10xMajorPlusMinor(vulkanVer);
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u, "Mojor Vulkan version must be from 1 to 9");
			globalAppFlags.vulkanVer.update(version);
			vulkanVer = version;
		}
	}

	uint32_t	spirvVer = Version::make(1, 0);
	consumeRes = consumeOptions(spirv, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << spirv.name << "\" (spirv compile target) option param" << std::endl;
		return 2;
	}
	else if (consumeRes > 0)
	{
		spirvVer = fromText(sink.back(), 10u, status);
		if (!status)
		{
			std::cout << "Unable to parse SPIR-V version, default "
					  << globalAppFlags.spirvVer.nmajor << '.' << globalAppFlags.spirvVer.nminor
					  << " will be used" << std::endl;
		}
		else
		{
			const Version version = Version::from10xMajorPlusMinor(spirvVer);
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u, "Mojor SPIR-V version must be from 1 to 9");
			globalAppFlags.spirvVer.update(version);
			spirvVer = version;
		}
	}

	std::string assets;
	consumeRes = consumeOptions(optAssets, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optAssets.name << "\" (assets dir) option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()), "Given assets directory does not exist");
		assets = sink.back();
	}

	std::fill(std::begin(globalAppFlags.tmpDir), std::end(globalAppFlags.tmpDir), '\0');
	consumeRes = consumeOptions(optTempDir, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optTempDir.name << "\" (app temporary dir) option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()), "Given temp directory does not exist");
		std::copy_n(sink.back().begin(),
			std::min(sink.back().length(), ARRAY_LENGTH(globalAppFlags.tmpDir) - 1),
			std::begin(globalAppFlags.tmpDir));
	}

	globalAppFlags.assetsPath.assign(ASSETS_PATH);
	globalAppFlags.debugPrintfEnabled = consumeOptions(dprintf, options, appArgs, sink) > 0;
	globalAppFlags.spirvValidate = consumeOptions(dprintf, options, appArgs, sink) > 0;
	globalAppFlags.nowerror = (consumeOptions(nowerror, options, appArgs, sink) > 0);
	globalAppFlags.noWarning_VUID_Undefined = (consumeOptions(optLayNoVuid, options, appArgs, sink) > 0);
	consumeOptions(layer, options, appArgs, globalAppFlags.layers);

	setGlobalAppFlags(globalAppFlags);

	if (!((allArgs.end() != testNamePos) && findAndUpdateTestByName(testRecord, AllTestRecords, testNamePos->c_str())))
	{
		std::cout << "ERROR: Unable to find any test name in app parameters" << std::endl;
		printUsage(std::cout);
		return 1;
	}

	if (appArgs.size())
	{
		printUsage(std::cout);
		std::cout << "ERROR: One or more unrecognized command line options: \"" << appArgs[0] << "\"" << std::endl;
		return 1;
	}

	testRecord.assets = assets.length()
		? (fs::path(fs::path(assets)  / "").generic_u8string().c_str())
		: (fs::path(fs::path(ASSETS_PATH) / *testNamePos / "").generic_u8string().c_str());

	std::copy(std::next(testNamePos), allArgs.end(), std::back_inserter(testArgs));
	performTest = true;

	return 0;
}

int main (int argc, char* argv[])
{
	recordAllTests(AllTestRecords);
	
	TestRecord	testRecord;
	strings		testArgs;
	bool		performTest	(false);
	int			result		(0);

	try
	{
		result = parseParams(argc, argv, testRecord, testArgs, performTest);
		if (performTest) result = (*testRecord.call)(testRecord, testArgs);
	}
	catch (std::runtime_error& e)
	{
		std::cout << e.what();
		throw;
	}
	if (performTest)
	{
		std::cout << "The test \"" << testRecord.name << "\" " << (result == 0 ? "Passed" : "FAILED") << std::endl;
	}

	return result;
}

void printUsage(std::ostream& str)
{
	str << "Usage: app [options, ...] <test_name> [<test_param>,...]" << std::endl;
	str << "Application ptions:" << std::endl;
	str << "  -h, --help:               prints this help and exits" << std::endl;
	str << "  -t:                       prints available test names" << std::endl;
	str << "  -c:                       builds auto-complete command" << std::endl;
	str << "  -dl:                      prints available device list" <<std::endl;
	str << "  -d <id>:                  picks device by id, default is 0" <<std::endl;
	str << "  -de <id>                  prints extension for device of id, default is 0" << std::endl;
	str << "                            honors options -api and -l if present" << std::endl;
	str << "  -ll:                      prints available instance layer names" << std::endl;
	str << "  -l <layer> [-l <layer>]:  enable layer(s)" << std::endl;
	str << "  -l-no-vuid-undefined:     suppress layers(s) VUID_Undefined warning" << std::endl;
	str << "  -api <version>:           Vulkan API version to apply, (major * 10 + minor), default is 1.1" << std::endl;
	str << "  -assets <assets_dir>      change assets directory, default is ${REPO}/assets/<test_name>" << std::endl;
	str << "  -tmp <temp_dir>           change temp directory, default is system's temp directory" << std::endl;
	str << "  -verbose <level>          enable application diagnostic messages, default is 0 that means disabled" << std::endl;
	str << "  -dprintf:                 enable Debug Printf feature" << std::endl;
	str << "  -bt:                      enable backtrace" << std::endl;
	str << "Compiler options:" << std::endl;
	str << "  -vulkan <version>:        (major * 10 + minor), default is 10 aka vulkan1.0" << std::endl;
	str << "  -spirv <version>:         (major * 10 + minor), default is 10 aka spirv1.0" << std::endl;
	str << "                            Set the execution environment where the shaders will be executed in.\n"
	       "                            Defaults to :\n"
	       "                             * vulkan1.0 under --client vulkan<ver>\n"
	       "                             * opengl    under --client opengl<ver>\n"
	       "                             * spirv1.0  under --target - env vulkan1.0\n"
	       "                             * *spirv1.3  under --target - env vulkan1.1\n"
	       "                             * *spirv1.5  under --target - env vulkan1.2\n"
	       "                             Multiple --target - env can be specified.\n";
	str << "  -spvvalid:                enable SPIR-V assembly validation" << std::endl;

	str << "  -nowerror:                allows warnig(s) from external compilators" << std::endl;
	str << "Available test(s):" << std::endl;
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
