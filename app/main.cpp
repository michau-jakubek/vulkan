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
#include "vtfOptionParser.hpp"
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

static DriverInitializer mainDriverInitlializer;

int parseParams (int argc, char* argv[], add_ref<TestRecord> testRecord, add_ref<strings> testArgs, add_ref<bool> performTest)
{
	performTest = false;

	GlobalAppFlags globalAppFlags;

	globalAppFlags.thisAppPath	= argv[0];
	globalAppFlags.vtfVer		= CurrentVtfVersion;

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

	Option help3{ "--print-global-help", 0 };

	bool status;
	strings sink;
	std::vector<Option>	options { help3 };

	if (consumeOptions(help3, options, allArgs, sink) > 0)
	{
		printUsage(std::cout);
		return 0;
	}

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

	Option help1{ "-h", 0 };					options.push_back(help1);
	Option help2{ "--help", 0 };				options.push_back(help2);
	Option testList{ "-t", 0 };					options.push_back(testList);
	Option complCmd{ "-c", 0 };					options.push_back(complCmd);
	Option devList{ "-dl", 0 };					options.push_back(devList);
	Option extList{ "-de", 1 };					options.push_back(extList);
	Option excludeDevExt{ "-ede", 1 };			options.push_back(excludeDevExt);
	Option curDev{ "-d", 1 };					options.push_back(curDev);
	Option addSingleLayer{ "-l", 1 };			options.push_back(addSingleLayer);
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
	Option optVtfVersion21{ "-v", 0 };			options.push_back(optVtfVersion21);
	Option optVtfVersion1{ "-vv", 0 };			options.push_back(optVtfVersion1);
	Option optVtfVersion0{ "-vvv", 0 };			options.push_back(optVtfVersion0);
	Option optVtfVersion22{ "--version", 0 };	options.push_back(optVtfVersion22);

	if (consumeOptions(verbose, options, appArgs, sink) > 0)
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

	consumeRes = consumeOptions(optVulkanDriver, options, appArgs, sink);
	if (consumeRes < 0)
	{
		std::cout << "ERROR: Missing \"" << optVulkanDriver.name << "\" option param" << std::endl;
	}
	else if (consumeRes > 0)
	{
		ASSERTMSG(fs::exists(sink.back()) && fs::is_regular_file(sink.back()), "Given Vulkan driver file doesn't exist");
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

	if ((consumeOptions(optVtfVersion21, options, appArgs, sink) > 0)
		|| (consumeOptions(optVtfVersion22, options, appArgs, sink) > 0))
	{
		printVtfVersion(std::cout, 2);
		return 0;
	}
	if (consumeOptions(optVtfVersion1, options, appArgs, sink) > 0)
	{
		printVtfVersion(std::cout, 1);
		return 0;
	}
	if (consumeOptions(optVtfVersion0, options, appArgs, sink) > 0)
	{
		printVtfVersion(std::cout, 0);
		return 0;
	}

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
		printLayersAndExtensions(std::cout);
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

	if (consumeOptions(compilerIndex, options, appArgs, sink) > 0)
	{
		globalAppFlags.compilerIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "WARNING: Unable to parse compiler index, default 0 value will be used" << std::endl;
		}
	}

	if (consumeOptions(btrace, options, appArgs, sink) > 0)
	{
		backtraceEnabled(fromText(sink.back(), 0, status) != 0);
	}

	// -api
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

	// -api-int variant,major,minor,patch
	consumeRes = consumeOptions(optApiInt, options, appArgs, sink);
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

	// -d
	if (consumeOptions(extList, options, appArgs, sink) > 0)
	{
		const uint32_t deviceIndex = fromText(sink.back(), 0u, status);
		if (!status)
		{
			std::cout << "ERROR: Unable to parse physical device index" << std::endl;
			return 1;
		}

		consumeOptions(addSingleLayer, options, appArgs, globalAppFlags.layers);
		if (const std::string envLayers = getenv(VK_INSTANCE_LAYERS); envLayers.length())
		{
			const auto envLayerList = splitString(envLayers, listSeparator);
			globalAppFlags.layers.insert(globalAppFlags.layers.end(), envLayerList.begin(), envLayerList.end());
		}
		if (consumeOptions(addAllAvailableLayers, options, appArgs, sink) > 0)
		{
			const auto availableLayers = enumerateInstanceLayers();
			globalAppFlags.layers.insert(globalAppFlags.layers.end(), availableLayers.begin(), availableLayers.end());
		}

		ZInstance			instance = createInstance(THIS_EXECUTABLE_NAME
													  , getAllocationCallbacks()
													  , globalAppFlags.layers	// const strings&	desiredLayers
													  , {}						// const strings&	desiredExtension
													  , apiVer					// apiVersion
													  , false					// enableDebugPrintf
													  );
		ZPhysicalDevice		phys = selectPhysicalDevice(make_signed(deviceIndex), instance, {/* required extensions */});
		add_cref<strings>	exts	= phys.getParamRef<ZDistType<AvailableDeviceExtensions, strings>>();
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
	globalAppFlags.spirvValidate = consumeOptions(spvValid, options, appArgs, sink) > 0;
	if (consumeOptions(spvValidArgs, options, appArgs, sink) > 0)
	{
		globalAppFlags.spirvValArgs = sink.back();
		globalAppFlags.spirvValidate = true;
	}
	globalAppFlags.genSpirvDisassembly = consumeOptions(spvDisassm, options, appArgs, sink) > 0;
	globalAppFlags.nowerror = (consumeOptions(nowerror, options, appArgs, sink) > 0);
	globalAppFlags.noWarning_VUID_Undefined = (consumeOptions(optLayNoVuid, options, appArgs, sink) > 0);
	consumeOptions(excludeDevExt, options, appArgs, globalAppFlags.excludedDevExtensions);
	consumeOptions(optSuppressVUID, options, appArgs, globalAppFlags.suppressedVUIDs);

	consumeOptions(addSingleLayer, options, appArgs, globalAppFlags.layers);
	if (const std::string envLayers = getenv(VK_INSTANCE_LAYERS); envLayers.length())
	{
		const auto envLayerList = splitString(envLayers, listSeparator);
		globalAppFlags.layers.insert(globalAppFlags.layers.end(), envLayerList.begin(), envLayerList.end());
	}
	if (consumeOptions(addAllAvailableLayers, options, appArgs, sink) > 0)
	{
		const auto availableLayers = enumerateInstanceLayers();
		globalAppFlags.layers.insert(globalAppFlags.layers.end(), availableLayers.begin(), availableLayers.end());
	}


	setGlobalAppFlags(globalAppFlags);

	if (!((allArgs.end() != testNamePos) && findAndUpdateTestByName(testRecord, AllTestRecords, testNamePos->c_str())))
	{
		printUsage(std::cout);
		std::cout << "ERROR: Unable to find any test name in app parameters" << std::endl;
		return 1;
	}

	if (appArgs.size())
	{
		printUsage(std::cout);
		std::cout << "ERROR: One or more unrecognized command line options: \"" << appArgs[0] << "\"" << std::endl;
		return 1;
	}

	testRecord.assets = ((assets.length() ? fs::path(assets) : fs::path(ASSETS_PATH)) / *testNamePos).string();

	std::copy(std::next(testNamePos), allArgs.end(), std::back_inserter(testArgs));
	performTest = true;

	return 0;
}

int main (int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG) && defined(CHECK_MEMORY_LEAKS)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif

	recordAllTests(AllTestRecords);
	
	TestRecord	testRecord;
	strings		testArgs;
	bool		performTest	(false);
	TriLogicInt result		(0);

	try
	{
		result = parseParams(argc, argv, testRecord, testArgs, performTest);
		if (performTest)
		{
			printVtfVersion(std::cout, 2);
			std::cout << std::endl;
			result = (*testRecord.call)(testRecord, testArgs);
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
	str << "  -de <id>                  prints extension for device of id, default is 0" << std::endl;
	str << "                            honors options -api and -l if present" << std::endl;
	str << "  -ede <ext> [-ede <ext>]:  don't load extension during device creation" << std::endl;
	str << "  -ll:                      prints available instance layer names" << std::endl;
	str << "  -l <layer> [-l <layer>]:  enable layer(s)" << std::endl;
	str << "                            also respects " VK_INSTANCE_LAYERS " environment variable" << std::endl;
	str << "  -lall                     enable all available layers" << std::endl;
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
	str << "  -compiler <index>:        set compiler index from looking it on the operating system, default is 0.\n"
		<< "                            If index is negative value then statically glslang compiler will be used,\n"
		<< "                            and project needs to be configured with OFFLINE_SHADER_COMPILER enabled." << std::endl;
	str << "  -nowerror:                allows warnig(s) from external compilators\n" << std::endl;
	str << "  NOTE: The app internally uses some of the Vulkan SDK tools e.g. glslangValidator or spirv-val\n"
		<< "        so these have to be visible to it. In order to find where certain tool sits the app\n"
		<< "        uses \"where\" command on Windows or \"type\" on Linux, so these also must have to\n"
		<< "        be visible either as built-in shell command or standalone executable.\n" << std::endl;
	str << "Available test(s):" << std::endl;
	printAvailableTests(str, AllTestRecords, "\t", true);
}

std::string constructCompleteCommand (const char* appPath)
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
