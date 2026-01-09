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
};

template<int> struct TestRecorder
{
	static	bool record (TestRecord& out);
};

vtf::TriLogicInt launchTest (int argc, char** argv, add_cref<std::string> testName);
void recordAllTests (std::vector<TestRecord>& records);
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

#define USE_TEST_IDENTIFIER
#ifdef USE_TEST_IDENTIFIER
/** ******************************************************
 * @brief	The TestIdentifier enum
 * @note	Add your test id here and implement TestRecorded<id> class
 *********************************************************/
enum TestIdentifier
{
	ALL_TESTS_BEGIN,
	VTF_LAUNCHER,
	TRIANGLE,
	//VIEWER,
	LINE_WIDTH,
	//MULTIVIEW,
	//INT_COMPUTE,
    NOTHING_COMPUTE,
	//INT_GRAPHICS,
	INT_MATRIX,
	//INT_CIPHER,
	INT_THREADPOOL,
	INT_SYNCHRONIZATION2,
	INT_GEOM,
	COGWHEELS,
	SUBGROUP_MATRIX,
	//TOPOLOGY,
#ifdef ENABLE_DAEMON_TEST
	DAEMON,
#endif
	//DEMOTE_INVOCATIONS,
	BLENDING,
	SPARSE_BUFFER,
	DEVICE_TIMEOUT,
	SHADER_OBJECT_TRIANGLE,
	SHADER_OBJECT_COMPUTE,
	DESCRIPTOR_BUFFER,
	COOPERATIVE_MATRIX,
	TASK_MESH_TRIANGLE,
	BANAL_DRLR,
	BANAL_RENDERPASS,
	MUTABLE_DESCRIPTOR,
	VARIABLE_POINTERS,
	//TRANSFORM_FEEDBACK,
	DEPTH,
	STRUCT_GENERATOR,
	REJTREJSING_INTRO,
	DEVICE_CRASH,
	FRACTALS,

	ALL_TESTS_END = FRACTALS
};
#else
static uint32_t globalTestIdentifier;
constexpr uint32_t nextTestIdentifier() { return ++globalTestIdentifier; }
#endif // USE_TEST_IDENTIFIER

/*************************************************
* ****** A template for creating a new test ******
* ************************************************
*
*  Include below excerpt in your source file in the global scope.
*  No need to declare forward declaration of TestRecorder<> content,
*  it is enough that you include "allTests.hpp" file in your heeder
*  file and thet include your header file in you source test file.

template<> struct TestRecorder<YOUR_TEST_ENUM>
{
	static bool record (TestRecord&);
};
bool TestRecorder<YOUR_TEST_ENUM>::record (TestRecord& record)
{
	record.name = "your_test_name";
	record.call = &prepareTests;
	return true;
}

*  Assume that YOUR_TEST_ENUM has been already added to above TestIdentifier enums.
*  A prepareTests() function is something kind of the main function of the tests
*  being created. It has not to be in the global scope, more over it would be the
*  best that you would put in an unnamed namespace or just any namespace to avoid
*  the conflicts with existing sources. Its signature is following:

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);

*  Both "TriLigic" and "string" are in "vtf" namespace so enable its visiblity
*  by "using namespace vtf;" or use fully qualified namea like "vtf::TriLogicInt"
*  and "vtf::strings". The "strings" is an alias for "std::vector<std::string>".
*  The "add_cref<X>" is a helper template which is expanded to "const X&".
*  I decided to define this because I worked with an editor that liked to remove
*  the "&" and "*" characters. I strongly use the references every time it is
*  necessay from an optimization point if view and the editor behaviour was
*  iritating to much. The "add_cref<>" and similar you can find in "vtfZDeletable.hpp" file.
**/

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
