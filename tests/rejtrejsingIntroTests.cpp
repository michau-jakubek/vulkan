#include "rejtrejsingIntroTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfContext.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfRTLayoutManager.hpp"
#include "vtfRTShaderCollection.hpp"
#include "vtfRTPipeline.hpp"
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
    bool buildAlways = false;
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

using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;
std::vector<ZShaderModule> genRayTracingShaders(ZDevice device, add_cref<SBTShaderGroup> group, add_cref<Params> params)
{
    RTShaderCollection coll(device, RTShaderCollection::HitGroupsOrder::NumberOrder, params.assets);
    SBTShaderGroup group0(group.next());
    SBTShaderGroup hitGroup0(group.next());
    SBTShaderGroup group1(group);
    SBTShaderGroup hitGroup1(group);
    SBTShaderGroup hitGroup2(hitGroup1.next());

    const std::string rgen0("shader0.rgen");
    const std::string rmiss0("shader0.rmiss");
    const std::string rcall0("shader0.rcall");
    const std::string rahit0("shader0.rahit");
    const std::string rchit0("shader0.rchit");
    const std::string rint0("shader0.rint");

    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    // 1 rgen, 3 miss, { any } = 5 shaders, 4x general, 1x hit = batch: 5

    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint0);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit0);
    coll.addFromFile(group1, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group1, VK_SHADER_STAGE_CALLABLE_BIT_KHR, rcall0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);

    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit0);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit0);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    // 1 rgen, 1 miss, 1 call, { ahit, chit, int }, { ahit, chit }
    // = 8 shaders, 3x general, 2x hit = batch: 5

    // SUM: 4+3 general + 2+1 hit = 10 pipelineGroups
    coll.buildAndVerify(Version(1, 2), Version(1, 4), true, false, params.buildAlways);
    return coll.getAllShaders();
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
    SBTShaderGroup shaderGroup;
    const std::vector<ZShaderModule> rtShaders = genRayTracingShaders(ctx.device, shaderGroup, params);

    RTLayoutManager         lm              (ctx.device);
    //lm.addBinding(as1, stage);
	//ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
        uint32_t x;
    } const                 pc              { 0u };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ /*dsLayout*/ }, ZPushRange<PC>(stage));
    ZPipeline				cPipeline       = createComputePipeline(pipelineLayout, shader);
    ZPipeline               rtPipeline      = createRayTracingPipeline(pipelineLayout, rtShaders);
    SBT sbt1(rtPipeline, shaderGroup);
    sbt1.buildOnce();

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
        commandBufferBindPipeline(cmd, cPipeline);
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
