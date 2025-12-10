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

TriLogicInt prepareTest(const TestRecord& record, const strings& cmdLineParams)
{
    UNREF(cmdLineParams);
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
    Params params(record.assets);

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        UNREF(caps);
    };

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

ZShaderModule genShader(ZDevice device, add_cref<Params> params)
{
    UNREF(device);
    UNREF(params);
    return {};
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    ASSERTFALSE("Do not run this test!!!");

    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = genShader(ctx.device, params);

    LayoutManager           lm              (ctx.device);
	//ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
    } const                 pc              { };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ /*dsLayout*/ }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);


	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, pipeline);
        commandBufferPushConstants(cmd, pipelineLayout, pc);
		commandBufferDispatch(cmd);
	}

    return {};
}

} // unnamed namespace

template<> struct TestRecorder<NOTHING_COMPUTE>
{
	static bool record(TestRecord&);
};
bool TestRecorder<NOTHING_COMPUTE>::record(TestRecord& record)
{
	record.name = "nothing_compute";
	record.call = &prepareTest;
	return true;
}
