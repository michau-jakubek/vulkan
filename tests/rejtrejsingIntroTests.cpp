#include "rejtrejsingIntroTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfRTLayoutManager.hpp"
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
        onEnablingRTFeatures(caps);
    };

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

ZShaderModule genShader(ZDevice device, add_cref<Params> params)
{
    UNREF(device);
    UNREF(params);
    ProgramCollection p(device, params.assets);
    p.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
    p.buildAndVerify(true);
    return p.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    //ASSERTFALSE("Do not run this test!!!");

    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = genShader(ctx.device, params);

    std::vector<Vec3> vertices{ Vec3(-1, -1, 0), Vec3(+1, -1, 0), Vec3(0, +1, 0) };
    std::vector<uint32_t> indices{ 0, 1, 2 };
    ZAccelerationStructureGeometry geom1 = makeTrianglesGeometry(ctx.device, vertices, indices);
    ZBtmAccelerationStructure btm = createBtmAccelerationStructure({ &geom1 });
    ZTopAccelerationStructure top = createTopAccelerationStructure(BLAS(btm));
    RTLayoutManager         lm              (ctx.device);
    //lm.addBinding(as1, stage);
	//ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
        uint32_t x;
    } const                 pc              { 0u };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ /*dsLayout*/ }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);


	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
        commandBufferBindPipeline(cmd, pipeline);
        commandBufferBuildAccelerationStructure(cmd, top);
        commandBufferPushConstants(cmd, pipelineLayout, pc);
		commandBufferDispatch(cmd);
	}

    return {};
}

} // unnamed namespace

template<> struct TestRecorder<REJTREJSING_INTRO>
{
	static bool record(TestRecord&);
};
bool TestRecorder<REJTREJSING_INTRO>::record(TestRecord& record)
{
	record.name = "rejtrejsing_intro";
	record.call = &prepareTest;
	return true;
}
