#include <cstring>
#include "vtfZUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfCUtils.hpp"
#include "allTests.hpp"

std::vector<TestRecord> AllTestRecords;

TestRecord::TestRecord ()
	: name		()
	, desc		()
	, assets	()
	, call		()
{
}

void TestRecord::reset ()
{
	name = nullptr;
	desc = nullptr;
	call = nullptr;
}

void TestRecord::valid () const
{
	ASSERTION(name != nullptr);
	ASSERTION(call != nullptr);
}

#ifdef USE_TEST_IDENTIFIER
template<> struct TestRecorder<ALL_TESTS_BEGIN>
#else
template<> struct TestRecorder<0>
#endif
{
	static bool record (TestRecord& out);
};

#ifdef USE_TEST_IDENTIFIER
bool TestRecorder<ALL_TESTS_BEGIN>::record (TestRecord&)
#else
bool TestRecorder<0>::record (TestRecord&)
#endif
{
	throw std::runtime_error("Do not call me directly");
}

template<int Tail_> void recordTailTest (TestRecord& rec, std::vector<TestRecord>& records)
{
	rec.reset();
	if (TestRecorder<Tail_>::record(rec))
	{
		rec.valid();
		records.emplace_back(rec);
	}
	recordTailTest<Tail_-1>(rec, records);
}
template<> void recordTailTest<ALL_TESTS_BEGIN>(TestRecord&, std::vector<TestRecord>&) { }

void assertUiqueTest (const std::vector<TestRecord>& records)
{
	for (auto i = records.begin(); i != records.end(); ++i)
	{
		std::string msg("Two or more tests with the same name detected: \'");
		msg += i->name;
		msg += "\'";
		for (auto j = std::next(i); j != records.end(); ++j)
		{
			ASSERTMSG(!!std::strcmp(i->name, j->name), msg);
		}
	}
}

void recordAllTests (std::vector<TestRecord>& records)
{
	TestRecord rec;
#ifdef USE_TEST_IDENTIFIER
	recordTailTest<ALL_TESTS_END>(rec, records);
#else
	recordTailTest<globalTestIdentifier>(rec, records);
#endif
	assertUiqueTest(records);
}

std::vector<const char*> getTestNames (const std::vector<TestRecord>& records)
{
	std::vector<const char*> names;
	names.reserve(records.size());
	for (const auto& rec : records)
	{
		ASSERTION(rec.name);
		names.push_back(rec.name);
	}
	return names;
}

std::ostream& printAvailableTests (std::ostream& str, const std::vector<TestRecord>& records, const char* indent, bool includeNewLine)
{
	for (std::size_t i = 0; i < records.size(); ++i)
	{
		ASSERTION(records[i].name);
		if (i) str << std::endl;
		str << indent << records[i].name;
	}
	if (includeNewLine) str << std::endl;
	return str;
}

void printAvailableTests (const std::vector<TestRecord>& records, const char* indent)
{
	printAvailableTests(std::cout, records, indent, true);
}

bool findAndUpdateTestByName (TestRecord& rec, const std::vector<TestRecord>& records, const char* name)
{
	for (auto& test : records)
	{
		if (std::strcmp(test.name, name) == 0)
		{
			rec.name = test.name;
			rec.desc = test.desc;
			rec.call = test.call;
			return true;
		}
	}
	return false;
}

template<class T> T parseParam(const vtf::strings& params, uint32_t index, const T& defResult, bool& status)
{
	T result(defResult);
	if (index < params.size())
	{
		result = vtf::fromText(params[index], defResult, status);
	}
	else status = false;
	return result;
}

template<class T> T parseParam(const vtf::strings& params, uint32_t index, const T& defResult)
{
	bool status = false;
	return parseParam(params, index, defResult, status);
}

template int parseParam(const vtf::strings& params, uint32_t index, const int& defResult);
template float parseParam(const vtf::strings& params, uint32_t index, const float& defResult);
template std::string parseParam(const vtf::strings& params, uint32_t index, const std::string& defResult);
template int parseParam(const vtf::strings& params, uint32_t index, const int& defResult, bool& status);
template float parseParam(const vtf::strings& params, uint32_t index, const float& defResult, bool& status);
template std::string parseParam(const vtf::strings& params, uint32_t index, const std::string& defResult, bool& status);

template<class T> void warnParamDefault(const std::string& paramName, const T& defaultValue, bool status)
{
	if (status)
		std::cout << "STATUS:  Read " << paramName << " of " << defaultValue << " from command line" << std::endl;
	else
		std::cout << "WARNING: Unable to parse " << paramName << " at input, default of " << defaultValue << " is used" << std::endl;
}

template void warnParamDefault(const std::string& paramName, const int& defaultValue, bool status);
template void warnParamDefault(const std::string& paramName, const float& defaultValue, bool status);
template void warnParamDefault(const std::string& paramName, const std::string& defaultValue, bool status);

template<class T> bool warnParamRange(bool condition, const std::string& paramName,
									  T& paramValue, const T& paramMin, const T& paramMax,
									  const T& defaultValue, bool changeToDefault)
{
	bool result = true;
	if (!condition)
	{
		std::cout << "WARNING: Parameter " << paramName << "= " << paramValue
				  << " should be between " << paramMin << " and " << paramMax << std::endl;
		if (changeToDefault)
		{
			std::cout << "STATUS:  Setting the parameter " << paramName << " to " << defaultValue << std::endl;
			paramValue = defaultValue;
		}
		result = false;
	}
	return result;
}
template bool warnParamRange(bool, const std::string&, int&, const int&, const int&, const int&, bool);
template bool warnParamRange(bool, const std::string&, float&, const float&, const float&, const float&, bool);


template<class T> bool warnParamRange(const std::string& paramName,
									  T& paramValue, const T& paramMin, const T& paramMax,
									  const T& defaultValue, bool changeToDefault)
{
	return warnParamRange(vtf::between<T>(paramValue, paramMin, paramMax), paramName,
						  paramValue, paramMin, paramMax, defaultValue, changeToDefault);
}
template bool warnParamRange(const std::string&, int&, const int&, const int&, const int&, bool);
template bool warnParamRange(const std::string&, float&, const float&, const float&, const float&, bool);



