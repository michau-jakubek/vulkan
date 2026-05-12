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
    const VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bool inLiveCode = false;
    bool isArray = false;
    bool isDynamic() const;
    bool isUniform() const;
};
bool Params::isDynamic() const
{
    return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

bool Params::isUniform() const
{
    return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

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

std::string initPrograms(add_cref<Params> m_params)
{
    std::map<std::string, std::string> mapping;

    mapping["STORAGE_CLASS"] = m_params.isUniform() ? "Uniform" : "StorageBuffer";
    mapping["CONDITION_VAL"] = m_params.inLiveCode ? "OpConstantTrue" : "OpConstantFalse";

    if (m_params.isArray)
    {
        mapping["UNDEF_TYPE_DEF"] = "%len       = OpConstant %uint 4\n"
            "%ArrBuffer = OpTypeArray %MyBuffer %len";
        mapping["TARGET_TYPE"] = "%ArrBuffer";
        mapping["EXTRACT_OP"] = "%dummy     = OpCompositeExtract %v4float %param_undef 0 0";
    }
    else
    {
        mapping["UNDEF_TYPE_DEF"] = "";
        mapping["TARGET_TYPE"] = "%MyBuffer";
        mapping["EXTRACT_OP"] = "%dummy     = OpCompositeExtract %v4float %param_undef 0";
    }

    const std::string spirvTemplate = R"spirv(
    ; SPIR-V
    ; Version: 1.3
    OpCapability Shader
    OpMemoryModel Logical GLSL450
    OpEntryPoint GLCompute %main "main"
    OpExecutionMode %main LocalSize 1 1 1
    
    ; Typy bazowe
    %void      = OpTypeVoid
    %float     = OpTypeFloat 32
    %v4float   = OpTypeVector %float 4
    %uint      = OpTypeInt 32 0
    
    ; Definicja struktury bazowej
    %MyBuffer  = OpTypeStruct %v4float
    
    ; Dynamiczne wstrzyknięcie definicji tablicy (jeśli potrzebne)
    ${UNDEF_TYPE_DEF}
    
    ; Wskaźnik do bufora
    %ptr_Buffer = OpTypePointer ${STORAGE_CLASS} %MyBuffer
    
    ; Sygnatury funkcji
    %void_fn   = OpTypeFunction %void
    %helper_fn = OpTypeFunction %void ${TARGET_TYPE}
    
    ; OTO NASZ JEDYNY, SPARAMETRYZOWANY UNDEF
    %undef_res = OpUndef ${TARGET_TYPE}
    
    ; Warunki logiczne
    %bool      = OpTypeBool
    %condition = ${CONDITION_VAL} %bool
    
    ; --- FUNKCJA POMOCNICZA ---
    %process_undef = OpFunction %void None %helper_fn
    %param_undef   = OpFunctionParameter ${TARGET_TYPE}
    %helper_entry  = OpLabel
                     ${EXTRACT_OP}
                     OpReturn
                     OpFunctionEnd
    
    ; --- GŁÓWNA FUNKCJA ---
    %main      = OpFunction %void None %void_fn
    %entry     = OpLabel
                 OpSelectionMerge %merge None
                 OpBranchConditional %condition %then %merge
    
    %then      = OpLabel
    %dummy_res = OpFunctionCall %void %process_undef %undef_res
                 OpBranch %merge
    
    %merge     = OpLabel
                 OpReturn
                 OpFunctionEnd
    )spirv";

    const std::string spirvCode = subst_variables(spirvTemplate, mapping);
    return spirvCode;
}
ZShaderModule genShader(ZDevice device, add_cref<Params> params)
{
    const std::string shaderCode = initPrograms(params);
    //add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
    ProgramCollection programs(device, params.assets);
    programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, shaderCode);
    programs.buildAndVerify(Version(1, 3), Version(1, 3), true, true, true);
    return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = genShader(ctx.device, params);

    return 1;

    LayoutManager           lm              (ctx.device);
	//ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
    } const                 pc              { };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ /*dsLayout*/ }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(shader, pipelineLayout);


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
