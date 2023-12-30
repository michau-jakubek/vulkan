#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "subgroupMatrixTests.hpp"

namespace
{
using namespace vtf;

struct Params
{
};

int runSubgroupMatrixSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params);

int prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(record);
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	Params params;

	VulkanContext ctx(record.name, gf.layers, {}, {}, {}, gf.apiVer, gf.debugPrintfEnabled);

	return runSubgroupMatrixSingleThread(ctx, record.assets, params);
}

int runSubgroupMatrixSingleThread (VulkanContext& ctx, const std::string& assets, const Params& params)
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
