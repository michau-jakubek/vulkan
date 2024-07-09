#include "intThreadPool.hpp"
#include "vtfThreadPool.hpp"

namespace
{
using namespace vtf;

TriLogicInt runTest (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(record);
	UNREF(cmdLineParams);
	return ThreadPool::selfTest() ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<INT_THREADPOOL>
{
	static bool record(TestRecord&);
};
bool TestRecorder<INT_THREADPOOL>::record (TestRecord& record)
{
	record.name = "int_threadpool";
	record.call = &runTest;
	return true;
}

