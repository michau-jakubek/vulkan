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

bool Option::operator==(const Option& other) const
{
	if (name == nullptr && name == other.name)
		return other.follows == follows;
	return std::string(name).compare(other.name) == 0 && other.follows == follows;
}

static int consumeOptions (const Option& opt, const std::vector<Option>& opts, uint32_t arg, vtf::strings& args, vtf::strings& values, bool& error)
{
	if (arg >= args.size()) return 0;

	uint32_t	skip	= 0;
	int			found	= 0;

	if (std::strcmp(opt.name, args[arg].c_str()) == 0)
	{
		for (uint32_t follows = 0; follows < opt.follows; ++follows)
		{
			if (arg + 1 < args.size())
			{
				values.push_back(args[arg+1]);
				args.erase(std::next(args.begin(), arg+1));
			}
			else
			{
				error = true;
				break;
			}
		}
		args.erase(std::next(args.begin(), arg));
		found = error ? 0 : 1;
	}

	if (!error && found == 0)
	{
		skip = 1;
		for (auto& o : opts)
		{
			if (std::strcmp(args[arg].c_str(), o.name) == 0)
			{
				skip += o.follows;
				break;
			}
		}
	}

	return error ? 0 : (found + consumeOptions(opt, opts, arg+skip, args, values, error));
}

int consumeOptions (const Option& opt, const std::vector<Option>& opts, vtf::strings& args, vtf::strings& values)
{
	bool error = false;
	int count = consumeOptions(opt, opts, 0, args, values, error);
	return error ? (-1) : count;
}


