#include "deviceCrashTests.hpp"
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
#include "vtfCommandLine.hpp"

namespace
{
using namespace vtf;
using namespace sg;

constexpr Option optionPreserveInstance{ "--preserve-instance", 0 };
constexpr Option optionBuildAlways{ "--build-always", 0 };
constexpr Option optionRepeatCount{ "-repeat-count", 1 };
struct Params
{
    add_cref<std::string> assets;
    Params(add_cref<std::string> assets_) : assets(assets_) {}
    OptionParser<Params> getParser();
    uint32_t repeatCount = 2u;
    bool preserveInstance = false;
    bool buildAlways = false;
};
OptionParser<Params> Params::getParser()
{
    OptionFlags flagsNone{ OptionFlag::None };
    OptionFlags flagsDefault{ OptionFlag::PrintDefault };
    OptionParser<Params> p(*this);

    p.addOption(&Params::buildAlways, optionBuildAlways,
        "Force to (re)build shaders every time the application is run", { buildAlways }, flagsNone);

    p.addOption(&Params::preserveInstance, optionPreserveInstance,
        "Preserve VkInstance for consecutive runs", { preserveInstance }, flagsNone);

    p.addOption(&Params::repeatCount, optionRepeatCount,
        "Run the test N times", { repeatCount }, flagsDefault);

    return p;
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_ref<ZShaderModule> module,
                        add_cref<Params> params, const uint32_t testNumber);

TriLogicInt prepareTest(const TestRecord& record, add_ref<CommandLine> cmdLine)
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

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        UNREF(caps);
    };

    ZInstance instance;
    ZShaderModule module;
    ZPhysicalDevice physicalDevice;
    std::vector<TriLogicInt> results(params.repeatCount);

    for (uint32_t run = 0u; run < params.repeatCount; ++run)
    {
        if (params.repeatCount) {
            std::cout << "Run test " << run << std::endl;
        }

        if (params.preserveInstance)
        {
            if (0u == run)
            {
                VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
                physicalDevice = ctx.physicalDevice;
                instance = ctx.instance;
                results[run] = runTest(ctx, module, params, run);
            }
            else
            {
                ZDevice device = createLogicalDevice(physicalDevice, onEnablingFeatures, {}, gf.debugPrintfEnabled);
                VulkanContext ctx(instance, physicalDevice, device);
                results[run] = runTest(ctx, module, params, run);
            }
        }
        else
        {
            VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
            results[run] = runTest(ctx, module, params, run);
        }
        std::cout << "Status: "
            << (results[run].hasValue() ? (results[run].value() ? "FAIL" : "PASS") : "UNKNOWN") << std::endl;
    }

    return {};
}

ZShaderModule genShader (ZDevice device, add_cref<Params> params)
{
    ProgramCollection coll(device, params.assets);
    coll.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "device_crash.comp");
    coll.buildAndVerify(params.buildAlways);
    return coll.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_ref<ZShaderModule> module,
                        add_cref<Params> params, const uint32_t testNumber)
{
    UNREF(testNumber);

    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = module ? module : genShader(ctx.device, params);

    ZBuffer                 buffer          = createBuffer<uint32_t>(ctx.device, 16u);
    LayoutManager           lm              (ctx.device);
    lm.addBinding(buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    const uint32_t          dataIndex1      = 3u;
    const uint32_t          dataIndex2      = 5u;
    const uint32_t          dataValue1      = 133u;
    const uint32_t          dataValue2      = 157u;
    struct PC {
        uint32_t dataValue1, dataValue2, dataIndex1, dataIndex2;
    } const pc { dataValue1, dataValue2, dataIndex1, dataIndex2 };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, pipeline);
        commandBufferPushConstants(cmd, pipelineLayout, pc);
		commandBufferDispatch(cmd);
        {
            ZPipeline				pip = createComputePipeline(pipelineLayout, shader);
            commandBufferBindPipeline(cmd, pip);
        }
        cmd.endRecordingAndSubmit();
	}

    deviceWaitIdle(ctx.device);

    std::vector<uint32_t> data;
    lm.readBinding(0, data);
    std::cout << "Shader wrote "
        << data[dataIndex1] << " (expected " << dataValue1 << ") at position " << dataIndex1
        << " and "
        << data[dataIndex2] << " (expected " << dataValue2 << ") at position " << dataIndex2
		<< std::endl;

    return (dataValue1 == data[dataIndex1] && dataValue2 == data[dataIndex2]) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<DEVICE_CRASH>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DEVICE_CRASH>::record(TestRecord& record)
{
	record.name = "device_crash";
	record.call = &prepareTest;
	return true;
}
