#ifndef ALL_TESTS_HPP
#define ALL_TESTS_HPP

#include <vector>
#include <ostream>
#include "vtfVkUtils.hpp"

#define DEFINE_TEST(test_name_) \
	extern template bool TestRecorder<test_name_>::record (TestRecord&)

struct TestRecord
{
	const char*		name;			// test name (mandatory)
	const char*		desc;			// test description (mandatory)
	std::string		assets;			// given by caller
	vtf::strings	layers;			// given by caller
	uint32_t		deviceIndex;	// given by caller or default INVALID_UINT32
	int (*call)(const TestRecord& record, const vtf::strings& args);
	TestRecord ();
	void valid () const;
	void reset ();
};

template<int> struct TestRecorder
{
	static	bool record (TestRecord& out);
};

void recordAllTests (std::vector<TestRecord>& records);
void printAvailableTests (const std::vector<TestRecord>& records, const char* indent = "\t");
std::ostream& printAvailableTests (std::ostream& str, const std::vector<TestRecord>& records, const char* indent = "\t", bool includeNewLine = true);
bool findTestByName(TestRecord& rec, const std::vector<TestRecord>& records, const char* name);
std::vector<const char*> getTestNames(const std::vector<TestRecord>& records);

#define USE_TEST_IDENTIFIER
#ifdef USE_TEST_IDENTIFIER
/** ******************************************************
 * @brief	The TestIdentifier enum
 * @note	Add your test id here and implement TestRecorded<id> class
 *********************************************************/
enum TestIdentifier
{
	ALL_TESTS_ORIGIN,
    TRIANGLE,
	FRACTALS,

	ALL_TESTS_TAIL = FRACTALS
};
#else
static uint32_t globalTestIdentifier;
constexpr uint32_t nextTestIdentifier() { return ++globalTestIdentifier; }
#endif // USE_TEST_IDENTIFIER

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

struct Option
{
	const char* name;
	uint32_t follows;
};
int consumeOptions (const Option& opt, const std::vector<Option>& opts, vtf::strings& args, vtf::strings& values);

#endif // ALL_TESTS_HPP
