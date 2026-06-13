#ifndef ALL_TESTS_HPP
#define ALL_TESTS_HPP

#include <vector>
#include <ostream>
#include <memory>
#include "vtfVkUtils.hpp"

#define DEFINE_TEST(test_name_) \
	extern template bool TestRecorder<test_name_>::record (TestRecord&)

namespace vtf
{
	class CommandLine;
}

struct TestRecord
{
	const char*		name;			// test name (mandatory)
	const char*		desc;			// test description (mandatory)
	std::string		assets;			// given by caller
	vtf::TriLogicInt (*call)(add_cref<TestRecord> record, add_ref<vtf::CommandLine> args);
	bool			visible = true;
	TestRecord ();
	void valid () const;
	void reset ();

    static const std::vector<TestRecord>& allTestRecords();
};

// enum TestIdentifier definition
#include "allTests.inl"

template<int> struct TestRecorder
{
	static	bool record (TestRecord& out);
};

vtf::TriLogicInt launchTest (int argc, char** argv, add_cref<std::string> testName);
void printAvailableTests (const std::vector<TestRecord>& records, const char* indent = "\t");
std::ostream& printAvailableTests (std::ostream& str, const std::vector<TestRecord>& records, const char* indent = "\t", bool includeNewLine = true);
bool findAndUpdateTestByName (TestRecord& rec, const std::vector<TestRecord>& records, const char* name);
std::vector<const char*> getTestNames (const std::vector<TestRecord>& records);

class Shell;
typedef std::function<void(bool& doContinue, const vtf::strings& chunks, std::ostream& output)> OnShellCommand;
std::shared_ptr<Shell> getOrCreateUniqueShell(std::ostream&			output,
											  OnShellCommand		onCommand,
											  add_cref<std::string>	helpMessage = "This is help message",
											  add_cref<std::string>	historyFile = {});

// externals defined for int, float & std::string
template<class T> T parseParam(const vtf::strings& params, uint32_t index, const T& defResult);
template<class T> T parseParam(const vtf::strings& params, uint32_t index, const T& defResult, bool& status);
template<class T> void warnParamDefault(const std::string& paramName, const T& defaultValue, bool status);

template<class T> bool warnParamRange(bool condition, const std::string& paramName,
									  T& paramValue, const T& paramMin, const T& paramMax,
									  const T& defaultValue = {}, bool changeToDefault = true);
template<class T> bool warnParamRange(const std::string& paramName,
									  T& paramValue, const T& paramMin, const T& paramMax,
									  const T& defaultValue = {}, bool changeToDefault = true);

template<class T, class Comp> bool warnParamRange(const std::string& paramName,
												  T& paramValue, const T& paramMin, const T& paramMax, Comp comp,
												  const T& defaultValue = {}, bool changeToDefault = true)
{
	return warnParamRange(comp(paramValue, paramMin, paramMax), paramName, paramValue, paramMin, paramMax, defaultValue, changeToDefault);
}

#endif // ALL_TESTS_HPP
