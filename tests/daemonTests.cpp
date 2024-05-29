#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "daemonTests.hpp"
#include "daemon.hpp"
#include "shell.hpp"

static auto DaemonTestInstanceShellCreator = [](add_ref<std::ostream>	output,
												Shell::OnCommand		onCommand,
												add_cref<std::string>	helpMessage,
												add_cref<std::string>	historyFile)
											-> std::shared_ptr<Shell>
{
	return createOrGetSingletonShell(output, onCommand, helpMessage, historyFile);
};
typedef decltype(DaemonTestInstanceShellCreator) dtisc_t;

static auto DaemonTestInstanceDeviceCreator = [](add_cptr<char> name, bool graphical)
											-> vtf::SharedDevice
{
	return createOrGetSharedDevice(name, graphical);
};
typedef decltype(DaemonTestInstanceDeviceCreator) dtidc_t;

struct DaemonTestInstanceDetector
{
	dtisc_t shellCreator;
	dtidc_t deviceCreator;
	DaemonTestInstanceDetector(dtisc_t sc, dtidc_t dc)
		: shellCreator	(sc)
		, deviceCreator	(dc) {}
}
static const *ptrDaemonTestInstanceDetector;
std::shared_ptr<Shell> getOrCreateUniqueShell(std::ostream&			output,
											  OnShellCommand		onCommand,
											  add_cref<std::string>	helpMessage,
											  add_cref<std::string>	historyFile)
{
	if (ptrDaemonTestInstanceDetector)
		return ptrDaemonTestInstanceDetector->shellCreator(output, onCommand, helpMessage, historyFile);
	return std::make_shared<Shell>(output, onCommand, helpMessage, historyFile);
}

vtf::SharedDevice getOrCreateSharedDevice (add_cptr<char> name, bool graphical)
{
	if (ptrDaemonTestInstanceDetector)
		return ptrDaemonTestInstanceDetector->deviceCreator(name, graphical);
	return vtf::SharedDevice(name, graphical);
}

namespace
{
using namespace vtf;

struct Params : DaemonTestInstanceDetector
{
	std::string	history;
	bool		notSurface;
	const char*	thisTestName;
	Params ();
	void usage (add_ref<std::ostream> log) const;
	static std::tuple<bool,Params,std::string> parseCommandLine (const TestRecord& record, add_cref<strings> cmdLineParams);
};
Params::Params ()
	: DaemonTestInstanceDetector(DaemonTestInstanceShellCreator, DaemonTestInstanceDeviceCreator)
	, history		()
	, notSurface	(false)
	, thisTestName	(nullptr)
{
}
void Params::usage (add_ref<std::ostream> log) const
{
	log << "Usage:\n"
		<< "  -history <file>>  enable commands history\n"
		<< "  -no-surface       disable of surface support for device,\n"
		<< "                    default surface support is enabled\n"
		<< std::endl;
}
std::tuple<bool,Params,std::string> Params::parseCommandLine (const TestRecord& record, add_cref<strings> cmdLineParams)
{
	Option optZ			{ "-z", 0 };
	Option optTest		{ "-test", 0 };
	Option optTriangle	{ "-triangle", 0 };
	Option optHistory	{ "-history", 1 };
	Option optNoSurface	{ "-no-surface", 0 };
	const std::vector<Option> opts { optTest, optNoSurface, optTriangle, optZ, optHistory };
	bool status {}; UNREF(status);
	strings sink, args(cmdLineParams);
	Params params;
	params.thisTestName = record.name;
	auto makeResult = [&](bool result, const auto&... texts) -> std::tuple<bool,Params,std::string>
	{
		std::ostringstream os;
		SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << texts)...));
		os.flush();
		return { result, params, os.str() };
	};
	if (consumeOptions(optHistory, opts, args, sink) > 0)
	{
		params.history = sink.back();
	}
	params.notSurface	= consumeOptions(optNoSurface, opts, args, sink);
	if (!args.empty())
	{
		params.usage(std::cout);
		return makeResult(false, "Unrecognized option or parameter", std::quoted(args.at(0)));
	}
	return makeResult(true);
}

TriLogicInt runDaemonTestsSingleThread (const std::string& assets, const Params& params);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(record);
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags(); UNREF(gf);

	const auto[status, params, msg] = Params::parseCommandLine(record, cmdLineParams);
	if (!status)
	{
		std::cout << msg << std::endl;
		return (-1);
	}

	ptrDaemonTestInstanceDetector = &params;
	const TriLogicInt result = runDaemonTestsSingleThread(record.assets, params);
	ptrDaemonTestInstanceDetector = nullptr;

	return result;
}
std::string makeHelpMessage(const Shell& shell)
{
	std::ostringstream os;
	os << "Enter:\n";
	os << " * help for help\n";
	if (shell.shellEnabled) os << " * shell to run shell command\n";
	os << " * a test name with its args to run it\n";
	os << " * exit, quit or just \'q\' to terminate\n";
	os.flush();
	return os.str();
}
TriLogicInt runDaemonTestsSingleThread (const std::string& assets, const Params& params)
{
	UNREF(assets);
	TriLogicInt result = 0;
	auto onCommand = [&](bool& doContinue, const Shell::strings& chunks, std::ostream& output) -> void
	{
		UNREF(doContinue);
		UNREF(output);
		TestRecord rec;
		add_cref<std::string> cmd = chunks.at(0);
		if (cmd == params.thisTestName)
		{
			output << "Sorry, the test " << std::quoted(cmd) << " is already running" << std::endl;
		}
		else if (findAndUpdateTestByName(rec, AllTestRecords, cmd.c_str()))
		{
			rec.assets = (fs::path(ASSETS_PATH) / cmd).string();
			try
			{
				result = (*rec.call)(rec, strings(std::next(chunks.begin()), chunks.end()));
			}
			catch (std::exception& e)
			{
				result = (-1);
				output << e.what();
			}
		}
		else
		{
			output << "Available tests:\n";
			printAvailableTests(std::cout, AllTestRecords, "\t", true);
			output << "Unknown command or test name: " << std::quoted(cmd) << std::endl;
		}
	};
	createOrGetSharedDevice(params.thisTestName, !params.notSurface);
	auto shell = createOrGetSingletonShell(std::cout, onCommand, {}, params.history);
	std::string helpMessage = makeHelpMessage(*shell);
	std::cout << helpMessage;
	shell->helpMessage(std::move(helpMessage));

	shell->run();

	return result;
}

} // unnamed namespace

template<> struct TestRecorder<DAEMON>
{
	static bool record (TestRecord&);
};
bool TestRecorder<DAEMON>::record (TestRecord& record)
{
	record.name = "daemon";
	record.call = &prepareTests;
	return true;
}
