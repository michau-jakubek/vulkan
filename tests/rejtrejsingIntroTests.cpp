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
#include "demangle.hpp"
#include "vtfStructGenerator.hpp"
#include "vtfPrettyPrinter.hpp"
#include "vtfCommandLine.hpp"
#include "vtfRenderDoc.hpp"

#include <algorithm>
#include <numeric>
#include <random>

namespace vtf
{
extern bool rejtrejsingSelfTest();
extern bool rejtrejsingSelfTestSBT(
    ZPipeline,
    RTShaderCollection::SBTShaderGroup,
    rtdetails::PipelineShaderGroupOrder,
    add_ref<std::string>);
}
extern void zzz();
namespace
{
using namespace vtf;
using namespace sg;

constexpr Option optionBuildAlways{ "--build-always", 0 };
constexpr Option optionPrintStamp{ "--print-stamp", 0 };
constexpr Option optionShaderBuildThreadCount{ "-shader-build-thread-count", 1 };
constexpr Option optionPipelineShaderGroupOrder{ "-pipeline-group-order", 1 };
constexpr Option optionSelfTest{ "-self-test", 1 };
constexpr Option optionMiss0Index{ "-miss0", 1 };
constexpr Option optionMiss1Index{ "-miss1", 1 };
constexpr Option optionTriangleZet{ "-triangle-z", 1 };
constexpr Option optionRenderDoc{ "--renderdoc", 0 };
struct Params
{
    add_cref<std::string> assets;
    Params(add_cref<std::string> assets_)
        : assets(assets_)
        , pipelineShaderGroupOrder(rtdetails::PipelineShaderGroupOrder::Default.toInt())
    {
    }
    OptionParser<Params> getParser();
    uint32_t pipelineShaderGroupOrder = 0u;
    uint32_t selfTest = INVALID_UINT32;
    uint32_t shaderBuildThreadCount = 1u;
    uint32_t miss0index = 0u;
    uint32_t miss1index = 0u;
    float triangleZet = 0.5f;
    bool buildAlways = false;
    bool printStamp = false;
    bool renderdoc = false;
    std::string formatShaderGroupOrder (add_cref<OptionT<uint32_t>> sender) const;
    uint32_t parseShaderGroupOrder (
        add_cref<std::string>		text,
        add_cref<OptionT<uint32_t>>	sender,
        add_ref<bool>				status,
        add_ref<OptionParserState>	state) const;
};
std::string Params::formatShaderGroupOrder (add_cref<OptionT<uint32_t>> sender) const
{
    ASSERTMSG(std::string_view(optionPipelineShaderGroupOrder.name) == std::string_view(sender.name), "???");
    return rtdetails::PipelineShaderGroupOrder(sender.m_storage).toString();
}
uint32_t Params::parseShaderGroupOrder (
    add_cref<std::string>		text,
    add_cref<OptionT<uint32_t>>	sender,
    add_ref<bool>				status,
    add_ref<OptionParserState>	state) const
{
    if (state.hasHelp) {
        return 0;
    }
    auto order = rtdetails::PipelineShaderGroupOrder::fromString(text, &status);
    MULTI_UNREF(sender, status, state);
    if (false == status)
    {
        state.messages << "ERROR: Unable to parse shader group order list from \"" << text << "\".";
        state.hasErrors = true;
    }
    return order.toInt();
}

OptionParser<Params> Params::getParser ()
{
    OptionFlags flagsNone{ OptionFlag::None };
    OptionFlags flagsDefault{ OptionFlag::PrintDefault };
    OptionParser<Params> p(*this);

    p.addOption(&Params::buildAlways, optionBuildAlways,
        "Force to (re)build shaders every time the application is run", { buildAlways }, flagsNone);

    p.addOption(&Params::shaderBuildThreadCount, optionShaderBuildThreadCount,
        "Set number of threads to build shaders. "
        "It makes a sense or first run or build always flag is enabled",
        { shaderBuildThreadCount }, flagsDefault);

    p.addOption(&Params::printStamp, optionPrintStamp,
        "Print stamp to the console", { printStamp }, flagsNone);

    p.addOption(&Params::renderdoc, optionRenderDoc,
        "Enable RenderDoc hooks. Run only under RenderDoc", { renderdoc }, flagsDefault);

    typename OptionT<uint32_t>::parse_cb cbParseShaderGroupOrder =
        std::bind(&Params::parseShaderGroupOrder, this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4);
    typename OptionT<uint32_t>::format_cb cbFormatShaderGroupOrder =
        std::bind(&Params::formatShaderGroupOrder, this, std::placeholders::_1);
    p.addOption(&Params::pipelineShaderGroupOrder, optionPipelineShaderGroupOrder,
        "Set pipeline shader group order as comma-separated or not list of \"R,H,M,C\", "
        "including ignore case, where R indicates general group with raygen shader, "
        "M general with miss, C general with callable and H indicates hit group. "
        "Internally each group is represented by ray-tracing shader thus potential "
        "error parse messages will involve to shader names",
        { pipelineShaderGroupOrder }, flagsDefault,
        cbParseShaderGroupOrder, cbFormatShaderGroupOrder)
        ->setTypeName("text")
        ->setDefault(rtdetails::PipelineShaderGroupOrder(pipelineShaderGroupOrder).toString());
    p.addOption(&Params::selfTest, optionSelfTest, "Perform self test", { selfTest }, flagsNone,
        cbParseShaderGroupOrder, cbFormatShaderGroupOrder)
        ->setTypeName("pipeline-group-order")
        ->setDefault(std::string());

    p.addOption(&Params::miss0index, optionMiss0Index, "Miss 0 index", { miss0index }, flagsDefault);
    p.addOption(&Params::miss1index, optionMiss1Index, "Miss 1 index", { miss1index }, flagsDefault);

    p.addOption(&Params::triangleZet, optionTriangleZet, "Triangle Z coordinate", { triangleZet }, flagsDefault);

    return p;
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params);

void onEnablingFeatures (add_ref<DeviceCaps> caps)
{
    onEnablingRayTracingFeatures(caps);
};

TriLogicInt prepareTest (add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

    Params params(record.assets);
    auto parser = params.getParser();
    parser.parse(cmdLine);

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

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

//ZShaderModule genShader(ZDevice device, add_cref<Params> params)
//{
//    UNREF(device);
//    UNREF(params);
//    ProgramCollection p(device, params.assets);
//    p.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
//    p.buildAndVerify(true);
//    return p.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
//}

using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;
std::vector<ZShaderModule> genRayTracingShaders(ZDevice device, add_cref<SBTShaderGroup> group, add_cref<Params> params)
{
    add_ref<ProgressRecorder> progressRecorder =
        device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();

    RTShaderCollection coll(device, params.assets);
    SBTShaderGroup group0(group.next());
    SBTShaderGroup hitGroup0(group.next());
    SBTShaderGroup group1(group);
    SBTShaderGroup hitGroup1(group);
    SBTShaderGroup hitGroup2(hitGroup0.next());

    const std::string rgen0("shader0.rgen");
    const std::string rmiss0("shader0.rmiss");
    const std::string rcall0("shader0.rcall");
    const std::string rAhit0("shader0.rahit");
    const std::string rChit0("shader0.rchit");
    const std::string rint0("shader0.rint");

    const std::string rgen1("shader1.rgen");
    const std::string rmiss1("shader1.rmiss");
    const std::string rcall1("shader1.rcall");
    const std::string rAhit1("shader1.rahit");
    const std::string rChit1("shader1.rchit");
    const std::string rint1("shader1.rint");

    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    coll.addFromFile(group0, hitGroup0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rAhit0);
    coll.addFromFile(group0, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss0);
    // 1 rgen, 3 miss, { A,C } = 6 shaders, 4x general, 1x hit = batch: 5

    // coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint1);
    // coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_INTERSECTION_BIT_KHR, rint1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit1);

    coll.addFromFile(group1, hitGroup0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rAhit1);
    coll.addFromFile(group1, hitGroup0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit1);

    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit1);
    coll.addFromFile(group1, hitGroup2, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, rChit1);

    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen1);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rAhit1);
    coll.addFromFile(group1, hitGroup1, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, rAhit1);
    coll.addFromFile(group1, VK_SHADER_STAGE_MISS_BIT_KHR, rmiss1);
    coll.addFromFile(group1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgen1);
    // coll.addFromFile(group1, VK_SHADER_STAGE_CALLABLE_BIT_KHR, rcall0);
    // 1 rgen, 1 miss, { A,C }, { A,C }, { C }
    // = 7 shaders, 2x general, 3x hit = batch: 5

    // SUM: 4+2=6 general + 1+3=4 hit = 10 pipelineGroups, 13 shaders
    progressRecorder.stamp("Before buiding shaders");
    coll.buildAndVerify(Version(1, 4), Version(1, 4),
        true, false, params.buildAlways, {}, params.shaderBuildThreadCount);
    progressRecorder.stamp("After shaders build");

    return coll.getAllShaders();
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    RenderDoc rd;
    if (params.renderdoc)
    {
        std::cout << "RenderDoc varsion: " << rd.GetAPIVersion() << std::endl;
        rd.StartFrameCapture(ctx.instance);
    }

    add_ref<ProgressRecorder> progressRecorder =
        ctx.device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();

    //ZShaderModule			shader          = genShader(ctx.device, params);
    const float Z = params.triangleZet;
    std::vector<Vec3> vertices{ Vec3(-1, -1, Z), Vec3(+1, -1, Z), Vec3(0, +1, Z) };
    std::vector<uint32_t> indices{ 0, 1, 2 };
    ZAccelerationStructureGeometry geom1 = makeTrianglesGeometry(ctx.device, vertices, {});
    ZBtmAccelerationStructure btm = createBtmAccelerationStructure({ &geom1 });
    ZTopAccelerationStructure top = createTopAccelerationStructure(ctx.device, 10);
    SBTShaderGroup          shaderGroup0    (12);
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
    } pc{}; UNREF(pc);
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(VK_SHADER_STAGE_ALL));
    //ZPipeline				cPipeline       = createComputePipeline(pipelineLayout, shader);
    const auto              order           = rtdetails::PipelineShaderGroupOrder::fromInt(params.pipelineShaderGroupOrder);
    ZPipeline               rtPipeline      = createRayTracingPipeline(pipelineLayout, rtShaders, order);

    SBTHandles handles0(rtPipeline, shaderGroup0);
    SBT<Vec3, Vec4, uint32_t, uint32_t> sbt0(handles0);
    if (params.selfTest != INVALID_UINT32)
    {
        std::string errorMessages;
        if (const bool pass = rejtrejsingSelfTestSBT(rtPipeline, shaderGroup0,
            rtdetails::PipelineShaderGroupOrder(params.selfTest), errorMessages); false == pass)
            std::cout << errorMessages << std::endl;
        else std::cout << "Self test for group: " << shaderGroup0.groupIndex() << " PASS\n";
    }

    SBTHandles handles1(rtPipeline, shaderGroup1);
    SBT<Vec3, Vec4, uint32_t, uint32_t> sbt1(handles1);
    if (params.selfTest != INVALID_UINT32)
    {
        std::string errorMessages;
        if (const bool pass = rejtrejsingSelfTestSBT(rtPipeline, shaderGroup1,
            rtdetails::PipelineShaderGroupOrder(params.selfTest), errorMessages); false == pass)
                std::cout << errorMessages << std::endl;
        else std::cout << "Self test for group: " << shaderGroup1.groupIndex() << " PASS\n";
    }

    progressRecorder.stamp("Before command buffer recording");
    //for (uint32_t i = 0; i < 2u; ++i)
    {
        OneShotCommandBuffer cmd(cmdPool);
        commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, generalize0, generalize1);
        commandBufferBindPipeline(cmd, rtPipeline);
        commandBufferBuildAccelerationStructure(*cmd, top, BLAS(btm), { std::vector<BLAS>({BLAS(btm) }) });
        commandBufferPushConstants(cmd, pipelineLayout, PC{ 0u, 0u, params.miss0index });
        sbt1.traceRays(cmd, frameWidth, frameHeight);
        commandBufferPushConstants(cmd, pipelineLayout, PC{ 0u, 0u, params.miss1index });
        commandBufferTraceRays(cmd, sbt0, frameWidth, frameHeight);
        cmd.endRecordingAndSubmit(false, {}, 5'000'000'000ULL);
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

    if (params.renderdoc)
    {
        rd.EndFrameCapture(ctx.instance);
        std::cout << "Press Enter to finish application\n";
        std::cin.get();
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
