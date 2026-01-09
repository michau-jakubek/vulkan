#if defined(_MSC_VER) && defined(_DEBUG) && defined(CHECK_MEMORY_LEAKS)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#include "main.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfVector.hpp"
#include "vtfZUtils.hpp"
#include "vtfContext.hpp"
#include "vtfFilesystem.hpp"
#include "vtfLocale.hpp"
#include "allTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfPlatformDriver.hpp"
#include "vtf_git_version.inl"

#include <iostream>
#include <cstring>
#include <sstream>
#include <string_view>
#include <cstdlib>

using namespace vtf;

#define VK_INSTANCE_LAYERS "VK_INSTANCE_LAYERS"

static void printUsage (std::ostream& str);
static void printVtfVersion (std::ostream& str, uint32_t level);
static std::string constructCompleteCommand (const char* appPath);
extern void printLayersAndExtensions (add_ref<std::ostream> str);
extern void printCompilerList (add_ref<std::ostream> str);

static DriverInitializer mainDriverInitlializer;

int parseParams (
	add_ref<CommandLine> cmd,
	add_ref<TestRecord> testRecord,
	add_ref<std::string> testName,
	add_ref<bool> performTest)
{
	performTest = false;

	std::vector<TestRecord> allTestRecords;
	recordAllTests(allTestRecords);

	const auto allTestNames = [&] {
		const auto sTestNames = getTestNames(allTestRecords);
		strings rTestNames(sTestNames.size());
		std::transform(sTestNames.begin(), sTestNames.end(), rTestNames.begin(),
			[](const char* sTestName) { return std::string(sTestName); });
		return rTestNames;
		}();

	GlobalAppFlags globalAppFlags;
	globalAppFlags.thisAppPath = cmd.getAppName();
	globalAppFlags.vtfVer = CurrentVtfVersion;
	globalAppFlags.cmdSignature = cmd.getUserData();

	int consumeRes = (-1);

	Option help3{ "--print-global-help", 0 };

	bool status;
	strings sink;
	std::vector<Option>	options{ help3 };

	if (cmd.consumeOptions(help3, options, sink, allTestNames) > 0)
	{
		printUsage(std::cout);
		return 0;
	}

	Option help1{ "-h", 0 };					options.push_back(help1);
	Option help2{ "--help", 0 };				options.push_back(help2);
	Option testList{ "-t", 0 };					options.push_back(testList);
	Option complCmd{ "-c", 0 };					options.push_back(complCmd);
	Option devList{ "-dl", 0 };					options.push_back(devList);
	Option devExtList{ "-le", 1 };				options.push_back(devExtList);
	Option addDevExt{ "-e", 1 };				options.push_back(addDevExt);
	Option excludeDevExt{ "-rme", 1 };			options.push_back(excludeDevExt);
	Option curDev{ "-d", 1 };					options.push_back(curDev);
	Option vtfAsDllInstance{ "-i", 1 };			options.push_back(vtfAsDllInstance);
	Option addSingleLayer{ "-l", 1000 };		options.push_back(addSingleLayer);
	Option remSingleLayer{ "-rml", 1000 };		options.push_back(remSingleLayer);
	Option layList{ "-ll", 0 };					options.push_back(layList);
	Option addAllAvailableLayers{ "-lall", 0 };	options.push_back(addAllAvailableLayers);
	Option optLayNoVuid{ "-l-no-vuid-undefined", 0 };	options.push_back(optLayNoVuid);
	Option optSuppressVUID{ "-l-suppress", 1 };	options.push_back(optSuppressVUID);
	Option optAssets{ "-assets", 1 };			options.push_back(optAssets);
	Option optVulkanDriver{ "-driver", 1 };		options.push_back(optVulkanDriver);
	Option optTempDir{ "-tmp", 1 };             options.push_back(optTempDir);
	Option btrace{ "-bt", 0 };					options.push_back(btrace);
	Option optApi{ "-api", 1 };					options.push_back(optApi);
	Option optApiInt{ "-api-int", 1 };			options.push_back(optApiInt);
	Option vulkan{ "-vulkan", 1 };				options.push_back(vulkan);
	Option spirv{ "-spirv",  1 };				options.push_back(spirv);
	Option spvValid{ "-spvvalid", 0 };			options.push_back(spvValid);
	Option spvValidArgs{ "-spvvalargs", 1 };	options.push_back(spvValidArgs);
	Option spvDisassm{ "-spvdisassm", 0 };		options.push_back(spvDisassm);
	Option verbose{ "-verbose", 1 };			options.push_back(verbose);
	Option nowerror{ "-nowerror", 0 };			options.push_back(nowerror);
	Option dprintf{ "-dprintf", 0 };			options.push_back(dprintf);
	Option compilerIndex{ "-compiler", 1 };		options.push_back(compilerIndex);
	Option compilerList{ "-compiler-list", 0 };	options.push_back(compilerList);
	Option optVtfVersion21{ "-v", 0 };			options.push_back(optVtfVersion21);
	Option optVtfVersion1{ "-vv", 0 };			options.push_back(optVtfVersion1);
	Option optVtfVersion0{ "-vvv", 0 };			options.push_back(optVtfVersion0);
	Option optVtfVersion22{ "--version", 0 };	options.push_back(optVtfVersion22);

	if (cmd.consumeOptions(verbose, options, sink, allTestNames) > 0)
	{
		globalAppFlags.verbose = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse verbose level" << std::endl;
			return 1;
		}
		else
		{
			setGlobalAppFlags(globalAppFlags);
		}
	}

	consumeRes = cmd.consumeOptions(optVulkanDriver, options, sink, allTestNames);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optVulkanDriver.name << "\" option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()) && fs::is_regular_file(sink.back()),
			"Given Vulkan driver file \"", sink.back(), "\" doesn't exist");
		globalAppFlags.vulkanDriver = sink.back();
		setGlobalAppFlags(globalAppFlags);
	}

#ifndef VULKAN_CUSTOM_DRIVER
	vkGetInstanceProcAddr(nullptr, "vkGetInstanceProcAddr");
#endif
	mainDriverInitlializer(globalAppFlags.vulkanDriver, globalAppFlags.verbose);

#if SYSTEM_OS_WINDOWS
	const char listSeparator = ';';
#else
	const char listSeparator = ':';
#endif
	auto getenv = [](add_cptr<char> var) -> std::string
		{
#ifdef _MSC_VER
			char val[256]{};
			size_t cnt = 0;
			getenv_s(&cnt, val, ARRAY_LENGTH(val) - 1, var);
			return std::string(val);
#else
			if (char* val = std::getenv(var); val != nullptr)
				return std::string(val);
			return std::string();
#endif
		};

	if ((cmd.consumeOptions(optVtfVersion21, options, sink, allTestNames) > 0)
		|| (cmd.consumeOptions(optVtfVersion22, options, sink, allTestNames) > 0))
	{
		printVtfVersion(std::cout, 2);
		return 0;
	}
	if (cmd.consumeOptions(optVtfVersion1, options, sink, allTestNames) > 0)
	{
		printVtfVersion(std::cout, 1);
		return 0;
	}
	if (cmd.consumeOptions(optVtfVersion0, options, sink, allTestNames) > 0)
	{
		printVtfVersion(std::cout, 0);
		return 0;
	}

	if (cmd.consumeOptions(help1, options, sink, allTestNames) > 0
		|| cmd.consumeOptions(help2, options, sink, allTestNames) > 0)
	{
		printUsage(std::cout);
		return 0;
	}

	if (cmd.consumeOptions(testList, options, sink, allTestNames) > 0)
	{
		std::cout << "Available test list:" << std::endl;
		printAvailableTests(std::cout, allTestRecords, "\t", true);
		return 0;
	}

	if (cmd.consumeOptions(complCmd, options, sink, allTestNames) > 0)
	{
		std::cout << constructCompleteCommand(THIS_EXECUTABLE_NAME) << std::endl;
		return 0;
	}

	if (cmd.consumeOptions(devList, options, sink, allTestNames) > 0)
	{
		ZInstance instance = createInstance("test", getAllocationCallbacks());
		printPhysicalDevices(*instance, std::cout);
		return 0;
	}

	if (cmd.consumeOptions(layList, options, sink, allTestNames) > 0)
	{
		printLayersAndExtensions(std::cout);
		return 0;
	}

	if (cmd.consumeOptions(curDev, options, sink, allTestNames) > 0)
	{
		globalAppFlags.physicalDeviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}
	}

	if (cmd.consumeOptions(vtfAsDllInstance, options, sink, allTestNames) > 0)
	{
		globalAppFlags.vtfAsDllInstance = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse VTF_AS_DLL instance ID" << std::endl;
			return 1;
		}
	}

	if (cmd.consumeOptions(compilerIndex, options, sink, allTestNames) > 0)
	{
		globalAppFlags.compilerIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "WARNING: Unable to parse compiler index, default 0 value will be used" << std::endl;
		}
	}

	if (cmd.consumeOptions(compilerList, options, sink, allTestNames) > 0)
	{
		printCompilerList(std::cout);
		return 0;
	}

	if (cmd.consumeOptions(btrace, options, sink, allTestNames) > 0)
	{
		backtraceEnabled(fromText(sink.back(), 0, status) != 0);
	}

	// -api
	uint32_t	apiVer = Version::make(1, 1);
	consumeRes = cmd.consumeOptions(optApi, options, sink, allTestNames);
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
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u,
				"Mojor API version (", version.nmajor, ") must be from 1 to 9");
			globalAppFlags.apiVer.update(version);
			apiVer = version;
		}
	}

	// -api-int variant,major,minor,patch
	consumeRes = cmd.consumeOptions(optApiInt, options, sink, allTestNames);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optApiInt.name << "\" (Veulkan API version) option param" << std::endl;
		return 2;
	}
	else if (consumeRes > 0)
	{
		std::array<bool, 4> statuses;
		const UVec4 defApiInt(globalAppFlags.apiVer.nmajor, globalAppFlags.apiVer.nminor, 0, 0);
		const UVec4 apiInt = UVec4::fromText(sink.back(), defApiInt, statuses, &status);
		if (!status)
		{
			std::cout << "Unable to parse Vulkan API version, default "
				<< globalAppFlags.apiVer.nmajor << '.' << globalAppFlags.apiVer.nminor
				<< " will be used" << std::endl;
		}
		else
		{
			const Version version(apiInt.y(), apiInt.z(), apiInt.w(), apiInt.x());
			globalAppFlags.apiVer.update(version);
			apiVer = version;
		}
	}

	// -le
	if (cmd.consumeOptions(devExtList, options, sink, allTestNames) > 0)
	{
		const uint32_t deviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}

		cmd.consumeOptions(addSingleLayer, options, globalAppFlags.layers, allTestNames);
		if (const std::string envLayers = getenv(VK_INSTANCE_LAYERS); envLayers.length())
		{
			const auto envLayerList = splitString(envLayers, listSeparator);
			globalAppFlags.layers.insert(globalAppFlags.layers.end(), envLayerList.begin(), envLayerList.end());
		}
		if (cmd.consumeOptions(addAllAvailableLayers, options, sink, allTestNames) > 0)
		{
			const auto availableLayers = enumerateInstanceLayers();
			globalAppFlags.layers.insert(globalAppFlags.layers.end(), availableLayers.begin(), availableLayers.end());
		}
		strings removedLayers;
		if (cmd.consumeOptions(remSingleLayer, options, removedLayers, allTestNames) > 0)
		{
			removeStrings(removedLayers, globalAppFlags.layers);
		}

		ZInstance			instance = createInstance(THIS_EXECUTABLE_NAME
			, getAllocationCallbacks()
			, globalAppFlags.layers	// const strings&	desiredLayers
			, {}						// const strings&	desiredExtension
			, apiVer					// apiVersion
			, false					// enableDebugPrintf
		);
		ZPhysicalDevice		phys = selectPhysicalDevice(make_signed(deviceIndex), instance, {/* required extensions */ });
		add_cref<strings>	exts = phys.getParamRef<ZDistType<AvailableDeviceExtensions, strings>>();
		uint32_t			entry = 0;
		for (add_cref<strings::value_type> ext : exts)
		{
			if (entry) std::cout << '\n';
			std::cout << std::setw(3) << (entry++) << std::setw(0) << ": " << ext;
		}
		std::cout << std::endl;
		return 0;
	}

	uint32_t	vulkanVer = Version::make(1, 0);
	consumeRes = cmd.consumeOptions(vulkan, options, sink, allTestNames);
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
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u,
				"Mojor Vulkan version (", version.nmajor, ") must be from 1 to 9");
			globalAppFlags.vulkanVer.update(version);
			vulkanVer = version;
		}
	}

	uint32_t	spirvVer = Version::make(1, 0);
	consumeRes = cmd.consumeOptions(spirv, options, sink, allTestNames);
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
			ASSERTMSG(version.nmajor > 0u && version.nmajor <= 9u,
				"Major SPIR-V version (", version.nmajor, ") must be from 1 to 9");
			globalAppFlags.spirvVer.update(version);
			spirvVer = version;
		}
	}

	std::string assets;
	consumeRes = cmd.consumeOptions(optAssets, options, sink, allTestNames);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optAssets.name << "\" (assets dir) option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()), "Given assets directory \"", sink.back(), "\" does not exist");
		assets = sink.back();
	}

	std::fill(std::begin(globalAppFlags.tmpDir), std::end(globalAppFlags.tmpDir), '\0');
	consumeRes = cmd.consumeOptions(optTempDir, options, sink, allTestNames);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optTempDir.name << "\" (app temporary dir) option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()), "Given temp directory \"", sink.back(), "\" does not exist");
		std::copy_n(sink.back().begin(),
			std::min(sink.back().length(), ARRAY_LENGTH(globalAppFlags.tmpDir) - 1),
			std::begin(globalAppFlags.tmpDir));
	}

	globalAppFlags.assetsPath.assign(ASSETS_PATH);
	globalAppFlags.debugPrintfEnabled = cmd.consumeOptions(dprintf, options, sink, allTestNames) > 0;
	globalAppFlags.spirvValidate = cmd.consumeOptions(spvValid, options, sink, allTestNames) > 0;
	if (cmd.consumeOptions(spvValidArgs, options, sink, allTestNames) > 0)
	{
		globalAppFlags.spirvValArgs = sink.back();
		globalAppFlags.spirvValidate = true;
	}
	globalAppFlags.genSpirvDisassembly = cmd.consumeOptions(spvDisassm, options, sink, allTestNames) > 0;
	globalAppFlags.nowerror = (cmd.consumeOptions(nowerror, options, sink, allTestNames) > 0);
	globalAppFlags.noWarning_VUID_Undefined = (cmd.consumeOptions(optLayNoVuid, options, sink, allTestNames) > 0);
	cmd.consumeOptions(excludeDevExt, options, globalAppFlags.excludedDevExtensions, allTestNames);
	cmd.consumeOptions(optSuppressVUID, options, globalAppFlags.suppressedVUIDs, allTestNames);

	strings removedLayers;
	cmd.consumeOptions(addSingleLayer, options, globalAppFlags.layers, allTestNames);
	cmd.consumeOptions(remSingleLayer, options, removedLayers, allTestNames);
	if (const std::string envLayers = getenv(VK_INSTANCE_LAYERS); envLayers.length())
	{
		const auto envLayerList = splitString(envLayers, listSeparator);
		globalAppFlags.layers.insert(globalAppFlags.layers.end(), envLayerList.begin(), envLayerList.end());
	}
	if (cmd.consumeOptions(addAllAvailableLayers, options, sink, allTestNames) > 0)
	{
		const auto availableLayers = enumerateInstanceLayers();
		globalAppFlags.layers.insert(globalAppFlags.layers.end(), availableLayers.begin(), availableLayers.end());
	}
	removeStrings(removedLayers, globalAppFlags.layers);
	distinctStrings(globalAppFlags.layers);

	cmd.consumeOptions(addDevExt, options, globalAppFlags.deviceExtensions, allTestNames);
	distinctStrings(globalAppFlags.deviceExtensions);

	setGlobalAppFlags(globalAppFlags);

	const auto unconsumed = cmd.getUnconsumedTokens();
	testName = unconsumed.empty() ? std::string() : unconsumed[0];
	if (auto iTestName = std::find_if(allTestRecords.begin(), allTestRecords.end(),
		[&](add_cref<TestRecord> r) { return std::string_view(testName) == std::string_view(r.name); });
		testName.length() && iTestName != allTestRecords.end())
	{
		const Option optTestName(testName.c_str(), 0);
		options[0] = optTestName;
		ASSERTION(cmd.consumeOptions(optTestName, options, sink) == 1);
		cmd.truncateConsumed();
		options.clear();

		performTest = true;

		testRecord.name = iTestName->name;
		testRecord.desc = iTestName->desc;
		testRecord.call = iTestName->call;
		testRecord.assets = ((assets.length() ? fs::path(assets) : fs::path(ASSETS_PATH)) / testName).string();

		return 0;
	}

	if (testName.empty())
	{
		printUsage(std::cout);
		std::cout << "ERROR: Unable to find any test name in app parameters" << std::endl;
		return 1;
	}

	printUsage(std::cout);
	std::cout << "ERROR: One or more unrecognized command line options: \"" << testName << "\"" << std::endl;
	return 1;
}

int main (int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG) && defined(CHECK_MEMORY_LEAKS)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif

	if (argc < 2)
	{
		std::cout << "Pass available option as a parameter:" << std::endl;
		printUsage(std::cout);
		std::cout << "Available test list:" << std::endl;
		return 1;
	}

	std::string testName;
	TestRecord	testRecord;
	bool		performTest	(false);
	TriLogicInt result		(0);

	CommandLine cmdLine(argc, argv);

	try
	{
		result = parseParams(cmdLine, testRecord, testName, performTest);
		if (performTest)
		{
			printVtfVersion(std::cout, 2);
			std::cout << std::endl;
#if VTF_AS_DLL
			result = launchTest(argc, argv, testName);
#else
			result = (*testRecord.call)(testRecord, cmdLine);
#endif
		}
	}
	catch (std::runtime_error& e)
	{
		result = {};
		performTest = false;
		std::cout << e.what();
		if (testRecord.name)
			std::cout << "The test " << std::quoted(testRecord.name) << ' ';
		else
			std::cout << "Application ";
		std::cout << "thrown an exception so no result to show." << std::endl;
	}
	if (performTest)
	{
		std::cout << "The test " << std::quoted(testRecord.name) << ' '
			<< (result.hasValue()
				? (result == 0 ? "Passed" : "FAILED")
				: "finished but no results to show.") << std::endl;
	}

#if defined(_MSC_VER) && defined(_DEBUG) && defined(CHECK_MEMORY_LEAKS)
	_CrtDumpMemoryLeaks();
#endif

	return result.hasValue() ? result.value() : (1);
}

void printVtfVersion (std::ostream& str, uint32_t level)
{
	switch (level)
	{
	case 0:
		// -vvv
		str << VtfVersion(CurrentVtfVersion);
		break;
	case 1:
		// -vv
		str << "Vulkan Trivial Framework (VTF) ";
		str << "version " << VtfVersion(CurrentVtfVersion);
		break;
	case 2:
		// -v, --version
		str << "Vulkan Trivial Framework (VTF) ";
		str << "version: " << VtfVersion(CurrentVtfVersion) << '\n';
		{
			str << "Author: ";
			if (false == isPolishLocaleInstalled())
				str << "Michal";
			else
			{
				std::locale curLocale = str.getloc();
				std::locale tmpLocale = makeLocale(Locales::pl_PL);
				add_cptr<Diacritics> d = findDiacriticsByName(Locales::pl_PL, std::string());
				const wchar_t l = (d != nullptr) ? d->encode('l') : 'l';
				str.imbue(tmpLocale);
				str << "Micha" << WcharOrChar(l);
				str.imbue(curLocale);
			}
			str << ' ' << "Jakubek\n";
		}
		str << "E-mail: michau.jakubek@gmail.com\n";
		str << "Commit: " << VTF_GIT_VERSION << std::endl;
		break;
	}
}

void printUsage (std::ostream& str)
{
	printVtfVersion(str, 2);

	str << "Usage: app [options, ...] <test_name> [<test_param>,...]" << std::endl;
	str << "Application ptions:" << std::endl;
	str << "  -h, --help:               prints this help and exits" << std::endl;
	str << "  -v, --version:            prints program version info" << std::endl;
	str << "  -vv:                      prints program name and version number" << std::endl;
	str << "  -vvv:                     prints version number only" << std::endl;
	str << "  --print-global-help:      this option has global scope\n"
	    << "                            even if it exists after any test name or its params\n"
	    << "                            however the test can override it by itself" << std::endl;
	str << "  -t:                       prints available test names" << std::endl;
	str << "  -c:                       builds auto-complete command" << std::endl;
	str << "  -dl:                      prints available device list" <<std::endl;
	str << "  -d <id>:                  picks device by id, default is 0" <<std::endl;
	str << "  -i <id>                   define instance id. It makes a sense in VTF_AS_DLL build only.\n"
           "                             To learn more please read comments in source code." << std::endl;
	str << "  -le <id>                  prints extensions for device of id, default is 0" << std::endl;
	str << "                            honors options -api and -l if present" << std::endl;
	str << "  -e <ext> [-e <ext]        enable device extension(s)" << std::endl;
	str << "  -rme <ext> [-rme <ext>]:  don't load extension during device creation" << std::endl;
	str << "  -ll:                      prints available instance layer names" << std::endl;
	str << "  -l <layer> [-l <layer>]:  enable layer(s)" << std::endl;
	str << "                            also respects " VK_INSTANCE_LAYERS " environment variable" << std::endl;
	str << "  -lall                     enable all available layers" << std::endl;
	str << "  -rml <layer> [-rml ...]:  remove specific layer from enabled layers, usefull with -lall" << std::endl;
	str << "  -l-no-vuid-undefined:     suppress layers(s) VUID_Undefined warning" << std::endl;
	str << "  -l-suppress <VUID>:       suppress layers(s) specific VUID message: warning, error, etc.," << std::endl;
	str << "                            that match at least first 5 characters. No wildcard is applied." << std::endl;
	str << "                            e.g. \"-l-suppress VUID-vkBeginCommandBuffer-commandBuffer\"" << std::endl;
	str << "                            will match to VUID-vkBeginCommandBuffer-commandBuffer-00049" << std::endl;
	str << "  -api <version>:           Vulkan API version to apply, (major * 10 + minor), default is 1.1" << std::endl;
	str << "  -api-int <version>        Vulkan API version as (variant,major,minor,patch). If any, has higher priority than -api" << std::endl;
	str << "  -assets <assets_dir>      change assets directory, default is ${REPO}/assets/<test_name>" << std::endl;
	str << "  -tmp <temp_dir>           change temp directory, default is system's temp directory" << std::endl;
	str << "  -verbose <level>          enable application diagnostic messages, default is 0 that means disabled" << std::endl;
	str << "  -dprintf:                 enable Debug Printf feature" << std::endl;
	str << "                            #extension GL_EXT_debug_printf : enable and debugPrintfEXT(...)" << std::endl;
	str << "  -bt:                      enable backtrace" << std::endl;
	str << "  -driver <file>            change Vulkan driver. By defaault for Windows it is vulkan-1.dll and\n"
		   "                            for Linux is a library which you built with this app. To enable this functionality\n"
		   "                            remeber to enable VULKAN_CUSTOM_DRIVER." << std::endl;
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
	str << "  -spvvalid:                performs SPIR-V assembly validation, if failed then\n"
	    << "                            invalid binary file is saved as {hash}.{shader_file_name}.invalid.spvbin,\n"
	    << "                            otherwise spirv-val tool is launched anyway.\n"
	    << "                            glslang and spirv-val can give different results" <<std::endl;
	str << "  -spvvalargs:              text with semicolon separated list passed to spirv-val executable or as\n"
		<< "                            a parameter to static validator (if available, see -compiler).\n"
		<< "                            example: \"--relax-block-layout;--max-struct-depth 4;--relax-struct-store\"" << std::endl;
	str << "  -spvdisassm:              generate SPIR-V dissassembly file" << std::endl;
	str << "  -compiler-list:           prints list of compiler(s) available on PATH including static if any.\n"
		<< "                            An application assumes that required spirv-tools are available on that path.\n";
	str << "  -compiler <index>:        set compiler index from looking it on the operating system, default is 0.\n"
		<< "                            If index is negative value then statically glslang compiler will be used,\n"
		<< "                            and project needs to be configured with OFFLINE_SHADER_COMPILER enabled." << std::endl;
	str << "  -nowerror:                allows warnig(s) from external compilators\n" << std::endl;
	str << "  NOTE: The app internally uses some of the Vulkan SDK tools e.g. glslangValidator or spirv-val\n"
		<< "        so these have to be visible to it. In order to find where certain tool sits the app\n"
		<< "        uses \"where\" command on Windows or \"type\" on Linux, so these also must have to\n"
		<< "        be visible either as built-in shell command or standalone executable.\n" << std::endl;
	str << "Available test(s):" << std::endl;

	std::vector<TestRecord> tests;
	recordAllTests(tests);
	printAvailableTests(str, tests, "\t", true);
}

std::string constructCompleteCommand (const char* appPath)
{
	std::vector<TestRecord> allTestRecords;
	const auto names = getTestNames(allTestRecords);
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
