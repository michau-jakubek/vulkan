#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "subgroupMatrixTests.hpp"

namespace
{
using namespace vtf;

struct Params
{
};

TriLogicInt runSubgroupMatrixSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params);

TriLogicInt prepareTests (const TestRecord& record, add_ref<CommandLine> cmdLine)
{
	UNREF(record);
	UNREF(cmdLine);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	Params params;

	VulkanContext ctx(record.name, gf.layers, {}, {}, {}, gf.apiVer, gf.debugPrintfEnabled);

	return runSubgroupMatrixSingleThread(ctx, record.assets, params);
}

TriLogicInt runSubgroupMatrixSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params)
{
	UNREF(ctx);
	UNREF(assets);
	UNREF(params);
	return 0;
}

} // unnamed namespace

template<> struct TestRecorder<SUBGROUP_MATRIX>
{
	static bool record (TestRecord&);
};
bool TestRecorder<SUBGROUP_MATRIX>::record (TestRecord& record)
{
	record.name = "subgroup_matrix";
	record.call = &prepareTests;
	return true;
}
