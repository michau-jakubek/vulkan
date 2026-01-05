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
#include "vtfOptionParser.hpp"

namespace
{
using namespace vtf;
using namespace sg;

constexpr Option optionBuildAlways{ "--build-always", 0 };
constexpr Option optionPrintStamp{ "--print-stamp", 0 };
struct Params
{
    add_cref<std::string> assets;
    Params(add_cref<std::string> assets_) : assets(assets_) {}
    OptionParser<Params> getParser();
    bool buildAlways = false;
    bool printStamp = false;
};
OptionParser<Params> Params::getParser()
{
    OptionFlags flags{ OptionFlag::None };
    OptionParser<Params> p(*this);
    p.addOption(&Params::buildAlways, optionBuildAlways,
        "Force to (re)build shaders every time the application is run", { buildAlways }, flags);
    p.addOption(&Params::printStamp, optionPrintStamp,
        "Print stamp to the console", { printStamp }, flags);
    return p;
}

TriLogicInt runTest(add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTest(const TestRecord& record, const strings& cmdLineParams)
{
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

    Params params(record.assets);
    auto parser = params.getParser();
    parser.parse(cmdLineParams);

    add_cref<OptionParserState> state = parser.getState();
    if (state.hasHelp)
    {
        parser.printOptions(std::cout);
        return {};
    }
    if (state.hasErrors)
    {
        std::cout << "[ERROR] " << state.messagesText() << std::endl;
        return {};
    }

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        onEnablingRayTracingFeatures(caps);
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
    add_ref<ProgressRecorder> progressRecorder =
        device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();

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

    const std::string rgen1("shader1.rgen");
    const std::string rmiss1("shader1.rmiss");
    const std::string rcall1("shader1.rcall");
    const std::string rahit1("shader1.rahit");
    const std::string rchit1("shader1.rchit");
    const std::string rint1("shader1.rint");

    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    // 1 rgen, 3 miss, { any } = 5 shaders, 4x general, 1x hit = batch: 5

//    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint1);
//    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint1);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen1);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit1);
    coll.addFromFile(group1, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss1);
//    coll.addFromFile(group1, VK_SHADER_STAGE_CALLABLE_BIT_KHR, rcall0);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit1);

    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit1);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rahit1);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit1);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rchit1);
    // 1 rgen, 1 miss, 1 call, { ahit, chit, int }, { ahit, chit }
    // = 8 shaders, 3x general, 2x hit = batch: 5

    // SUM: 4+3 general + 2+1 hit = 10 pipelineGroups
    progressRecorder.stamp("Before buiding shaders");
    coll.buildAndVerify(Version(1, 4), Version(1, 4), true, false, params.buildAlways);
    progressRecorder.stamp("After shaders build");

    return coll.getAllShaders();
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    add_ref<ProgressRecorder> progressRecorder =
        ctx.device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();

    ZShaderModule			shader          = genShader(ctx.device, params);

    std::vector<Vec3> vertices{ Vec3(-1, -1, 0.5), Vec3(+1, -1, 0.5), Vec3(0, +1, 0.5) };
    std::vector<uint32_t> indices{ 0, 1, 2 };
    ZAccelerationStructureGeometry geom1 = makeTrianglesGeometry(ctx.device, vertices, indices);
    ZBtmAccelerationStructure btm = createBtmAccelerationStructure({ &geom1 });
    ZTopAccelerationStructure top = createTopAccelerationStructure(ctx.device, 10);
    SBTShaderGroup          shaderGroup0;
    SBTShaderGroup          shaderGroup1    (shaderGroup0.next());
    const std::vector<ZShaderModule> rtShaders = genRayTracingShaders(ctx.device, shaderGroup0, params);

    const uint32_t          frameWidth      = 8;
    const uint32_t          frameHeight     = 8;
    ZImage                  image0          = createImage(ctx.device, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D,
                                                frameWidth, frameHeight, ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT));
    ZImage                  image1          = createImage(ctx.device, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TYPE_2D,
                                                frameWidth, frameHeight, ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT));
    ZImageView              view0           = createImageView(image0);
    ZImageView              view1           = createImageView(image1);
    ZImageMemoryBarrier     generalize0     (image0, VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
    ZImageMemoryBarrier     generalize1     (image1, VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    ZCommandPool            cmdPool         = createCommandPool(ctx.device, ctx.computeQueue);
    RTLayoutManager         lm              (ctx.device, cmdPool);
    const uint32_t          asBinding       = lm.addBinding(top); UNREF(asBinding);
    const uint32_t          img0binding     = lm.addBinding(view0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
    const uint32_t          img1binding     = lm.addBinding(view1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
	ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
        uint32_t hitGroupsSbtOffset;
        uint32_t sbtStride;
        uint32_t missIndex;
    } pc{};
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(VK_SHADER_STAGE_ALL));
    ZPipeline				cPipeline       = createComputePipeline(pipelineLayout, shader);
    ZPipeline               rtPipeline      = createRayTracingPipeline(pipelineLayout, rtShaders);

    SBTHandles handles0(rtPipeline, shaderGroup0);
    SBT<Vec3, Vec4, uint32_t, uint32_t> sbt0(handles0);

    SBTHandles handles1(rtPipeline, shaderGroup1);
    SBT<Vec3, Vec4, uint32_t, uint32_t> sbt1(handles1);

    progressRecorder.stamp("Before command buffer recording");
    for (uint32_t i = 0; i < 2u; ++i)
    {
        OneShotCommandBuffer cmd(cmdPool);
        commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, generalize0, generalize1);
        commandBufferBindPipeline(cmd, rtPipeline);
        commandBufferPushConstants(cmd, pipelineLayout, pc);
        commandBufferBuildAccelerationStructure(*cmd, top, BLAS(btm), { std::vector<BLAS>({BLAS(btm) }) });
        sbt0.traceRays(cmd, frameWidth, frameHeight);
        sbt1.traceRays(cmd, frameWidth, frameHeight);
    }
    progressRecorder.stamp("After command buffer recording");
    
    auto printImage = [&](uint32_t binding) {
        std::cout << "Image" << binding << "\n-----------------\n";
        std::vector<Vec4> color(frameWidth * frameHeight);
        lm.readBinding(binding, color);
        for (uint32_t y = 0u; y < frameHeight; ++y)
        {
            for (uint32_t x = 0u; x < frameWidth; ++x)
            {
                Vec3 px = color[y * frameWidth + x].cast<Vec3>();;
                std::cout << px << ' ';
            }
            std::cout << std::endl;
        }
    };

    if (params.printStamp)
    {
        progressRecorder.print(std::cout);
        std::cout << std::endl;
    }

    printImage(img0binding);
    printImage(img1binding);

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
