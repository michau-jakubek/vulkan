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
#include "demangle.hpp"
#include "vtfStructGenerator.hpp"
#include "vtfPrettyPrinter.hpp"
#include "vtfCommandLine.hpp"
#include "vtf_app.hpp"

#include <Windows.h>
#include <numeric>
#include <random>

namespace
{
using namespace vtf;
using namespace sg;

struct Params
{
    add_cref<std::string> assets;
    add_ref<CommandLine> cmdLine;
    Params(add_cref<std::string> assets_, add_ref<CommandLine> cmdLine_)
        : assets(assets_)
        , cmdLine(cmdLine_) {}
};

TriLogicInt runTest(add_ref<Canvas> ctx, add_cref<Params> params);

TriLogicInt prepareTest(const TestRecord& record, add_ref<CommandLine> cmdLine)
{
    UNREF(cmdLine);
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
    Params params(record.assets, cmdLine);

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        UNREF(caps);
    };

    CanvasStyle canvasStyle = Canvas::DefaultStyle;
    canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
    Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle,
        onEnablingFeatures, gf.apiVer);

    return runTest(cs, params);
}

TriLogicInt runTest (add_ref<Canvas> ctx, add_cref<Params> params)
{
    UNREF(ctx); UNREF(params);
    add_ref<CommandLine> cmd = params.cmdLine;

    const std::string lib = "vtf.dll";

    HMODULE vtf = LoadLibraryA(lib.c_str());
    if (nullptr == vtf)
    {
        std::cout << "LoadLibraryA(\"" << lib << "\") FAILED\n";
        return 1;
    }
    pVTF_GetAPI VTF_GetAPI = (pVTF_GetAPI)GetProcAddress(vtf, VTF_GetAPI_PROC_NAME);
    if (nullptr == VTF_GetAPI)
    {
        std::cout << "GetProcAddress(\"" << VTF_GetAPI_PROC_NAME << "\") FAILED\n";
        FreeLibrary(vtf);
        return 1;
    }
    VTF_API* pVTF_API;
    add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

    add_cref<strings> rRequiredLayerExtensions =
        ctx.instance.getParamRef<ZDistType<RequiredLayerExtensions, std::vector<std::string>>>();
    std::vector<add_ptr<char>> sRequiredLayerExtensions(rRequiredLayerExtensions.size());
    for (size_t i = 0; i < rRequiredLayerExtensions.size(); ++i)
        sRequiredLayerExtensions[i] = const_cast<add_ptr<char>>(rRequiredLayerExtensions[i].c_str());
 
    (*VTF_GetAPI)(
        *ctx.physicalDevice,
        *ctx.instance,
        gf.vtfAsDllInstance,
        *ctx.device,
        gf.physicalDeviceIndex,
        sRequiredLayerExtensions.data(),
        data_count(sRequiredLayerExtensions),
        queueGetFamilyIndex(ctx.graphicsQueue),
        queueGetFamilyIndex(ctx.computeQueue),
        &pVTF_API);
    if (nullptr == pVTF_API)
    {
        std::cout << "VTF_GetAPI(...) FAILED\n";
        FreeLibrary(vtf);
        return 1;
    }

    uint32_t major, minor, patch, revision;
    pVTF_API->GetVersion(&major, &minor, &patch, &revision);
    const Version version(major, minor, patch, revision);
    std::cout << "Current VTF API version is " << VtfVersion(version) << std::endl;

    pVTF_API->RunTest(cmd.getMultiString().c_str());

    FreeLibrary(vtf);

    return 0;
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
