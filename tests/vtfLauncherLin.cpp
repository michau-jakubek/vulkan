#include "nothingCompute.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfFloat16.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include <numeric>
#include <random>
#include "demangle.hpp"
#include "vtfStructGenerator.hpp"
#include "vtfPrettyPrinter.hpp"

namespace
{
using namespace vtf;
using namespace sg;

struct Params
{
    add_cref<std::string> assets;
    Params(add_cref<std::string> assets_) : assets(assets_) {}
};

TriLogicInt runTest(add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTest(const TestRecord& record, add_ref<CommandLine> cmdLine)
{
    UNREF(cmdLine);
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
    Params params(record.assets);

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        UNREF(caps);
    };

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    return {};
}

} // unnamed namespace

template<> struct TestRecorder<VTF_LAUNCHER>
{
	static bool record(TestRecord&);
};
bool TestRecorder<VTF_LAUNCHER>::record(TestRecord& record)
{
	record.name = "vtf_launcher";
	record.call = &prepareTest;
    record.visible = false;
	return true;
}
