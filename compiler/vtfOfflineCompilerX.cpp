#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "spirv-tools/libspirv.hpp"
#include "vtfOfflineCompiler.hpp"
#include <vector>
#include <fstream>
#include <iostream>

static EShLanguage shaderStageToShLanguage (VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:					return EShLangVertex;
    case VK_SHADER_STAGE_FRAGMENT_BIT:					return EShLangFragment;
    case VK_SHADER_STAGE_COMPUTE_BIT:					return EShLangCompute;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return EShLangTessControl;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return EShLangTessEvaluation;
    case VK_SHADER_STAGE_GEOMETRY_BIT:					return EShLangGeometry;
    case VK_SHADER_STAGE_TASK_BIT_EXT:					return EShLangTask;
    case VK_SHADER_STAGE_MESH_BIT_EXT:					return EShLangMesh;
    default: break;
    }
    ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
    return EShLangVertex;
}

static glslang::EShTargetLanguageVersion spirvVerToShTargetLangVer (add_cref<vtf::Version> spirvVer)
{
    if (spirvVer.nmajor == 1u)
    {
        switch(spirvVer.nminor)
        {
        case 0u:
            return glslang::EShTargetSpv_1_0;
        case 1u:
            return glslang::EShTargetSpv_1_1;
        case 2u:
            return glslang::EShTargetSpv_1_2;
        case 3u:
            return glslang::EShTargetSpv_1_3;
        case 4u:
            return glslang::EShTargetSpv_1_4;
        case 5u:
            return glslang::EShTargetSpv_1_5;
        case 6u:
            return glslang::EShTargetSpv_1_6;
        }
    }
    ASSERTFALSE("Unknown SPIR-V version ", spirvVer.nmajor, '.', spirvVer.nminor);
    return glslang::EShTargetSpv_1_0;
}

static glslang::EShTargetClientVersion vulkanVerToShTargetCliVer (add_cref<vtf::Version> vulkanVer)
{
    if (vulkanVer.nmajor == 1u)
    {
        switch (vulkanVer.nminor)
        {
        case 0u:
            return glslang::EShTargetClientVersion::EShTargetVulkan_1_0;
        case 1u:
            return glslang::EShTargetClientVersion::EShTargetVulkan_1_1;
        case 2u:
            return glslang::EShTargetClientVersion::EShTargetVulkan_1_2;
        case 3u:
            return glslang::EShTargetClientVersion::EShTargetVulkan_1_3;
        case 4u:
            return glslang::EShTargetClientVersion::EShTargetVulkan_1_4;
        }
    }
    ASSERTFALSE("Unknown Vulkan client version ", vulkanVer.nmajor, '.', vulkanVer.nminor);
    return glslang::EShTargetClientVersion::EShTargetVulkan_1_0;
}

static spv_target_env makeSpvTargetEnv (add_cref<vtf::Version> vulkanVer, add_cref<vtf::Version> spirvVer)
{
    spv_target_env e = SPV_ENV_MAX;

    if (spirvVer.nminor < 5u)
    {
        switch (vulkanVer.nminor)
        {
        case 0u: e = SPV_ENV_VULKAN_1_0; break;
        case 1u: e = (spirvVer.nminor == 4u) ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1; break;
        case 2u: e = SPV_ENV_VULKAN_1_2; break;
        case 3u: e = SPV_ENV_VULKAN_1_3; break;
        case 4u: e = SPV_ENV_VULKAN_1_4; break;
        default: ASSERTFALSE("Unsupported Vulkan version ", vulkanVer.nmajor, '.', vulkanVer.nminor);
        }
    }

    if (SPV_ENV_MAX == e)
    {
        switch (spirvVer.nminor)
        {
        case 5u: e = SPV_ENV_UNIVERSAL_1_5; break;
        case 6u: e = SPV_ENV_UNIVERSAL_1_6; break;
        default: ASSERTFALSE("Unsupported SPIR-V version ", spirvVer.nmajor, '.', spirvVer.nminor);
        }
    }

    return e;
}

static add_cptr<TBuiltInResource> getDefaultTBuiltInResource ();

static bool validateSpirv (
    add_cref<std::vector<uint32_t>> binary,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_cref<std::string>           validationOptions,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder);

static bool generateDisassembly (
    add_cref<std::vector<uint32_t>> binary,
    add_cref<fs::path>              asmPath,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_ref<std::vector<char>>      disassembled,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder);

static int parseGlVersion (add_cref<std::string> shaderCode)
{
    int version = 0;
    const std::string versionToken("#version");
    std::istringstream i(shaderCode);
    for (int rep = 0; rep < 10; ++rep)
    {
        std::string token;
        i >> token;
        if (versionToken == token)
        {
            i >> version;
            break;
        }
    }
    return version;
}

static bool compileSpvShader (
    add_cref<std::string>           shaderCode,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder,
    add_ref<std::vector<uint32_t>>  binary);

namespace vtf
{

std::vector<char> compileShader (
    add_cref<std::string>           shaderCode,
    VkShaderStageFlagBits           shaderStage,
    add_cref<std::string>           entryPoint,
    bool                            isGlsl,
    bool                            enableValidation,
    add_cref<std::string>           validationOptions,
    bool                            genDisassmebly,
    add_cref<Version>               vulkanVer,
    add_cref<Version>               spirvVer,
    add_cref<fs::path>              binPath,
    add_cref<fs::path>              asmPath,
    add_ref<std::vector<char>>      disassembled,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder,
    add_ref<bool>                   status)
{
    std::vector<uint32_t> ispirv;

    if (isGlsl)
    {
        glslang::InitializeProcess();

        const EShLanguage lang = shaderStageToShLanguage(shaderStage);
        const int codeVersion = parseGlVersion(shaderCode);
        const EShMessages messages = EShMsgDefault;
        glslang::TShader shader(lang);
        add_cptr<char> strings[] = { shaderCode.c_str() };
        shader.setStrings(strings, 1);
        shader.setEnvTarget(glslang::EshTargetSpv, spirvVerToShTargetLangVer(spirvVer));
        shader.setEnvClient(glslang::EShClientVulkan, vulkanVerToShTargetCliVer(vulkanVer));
        shader.setSourceEntryPoint(entryPoint.c_str());
        shader.setEntryPoint(entryPoint.c_str());

        progressRecorder.stamp("Before glslang::TShader::parse()");
        if (!shader.parse(getDefaultTBuiltInResource(), codeVersion, false, messages)) {
            errorCollection << shader.getInfoLog() << std::endl;
            progressRecorder.stamp("glslang::TShader::parse() failed");
            status = false;
            return {};
        }
        progressRecorder.stamp("After glslang::TShader::parse()");

        glslang::TProgram program;
        program.addShader(&shader);

        progressRecorder.stamp("Before glslang::TProgram::link()");
        if (!program.link(messages)) {
            errorCollection << shader.getInfoLog() << std::endl;
            progressRecorder.stamp("glslang::TProgram::link() failed");
            status = false;
            return {};
        }
        progressRecorder.stamp("After glslang::TProgram::link()");

        glslang::GlslangToSpv(*program.getIntermediate(lang), ispirv);

        glslang::FinalizeProcess();
    }
    else
    {
        if (status = compileSpvShader(shaderCode, vulkanVer, spirvVer,
                        errorCollection, progressRecorder, ispirv); false == status)
            return {};
    }

    if (genDisassmebly)
    {
        if (status = generateDisassembly(ispirv, asmPath, vulkanVer, spirvVer, disassembled, errorCollection, progressRecorder),
            false == status) return {};
    }

    if (enableValidation)
    {
        if (status = validateSpirv(ispirv, vulkanVer, spirvVer, validationOptions, errorCollection, progressRecorder), false == status)
            return {};
    }

    status = true;
    std::vector<char> cspirv(ispirv.size() * sizeof(uint32_t));
    std::memcpy(cspirv.data(), ispirv.data(), vtf::data_count(cspirv));

    std::ofstream outBin(binPath, std::ios::binary);
    outBin.write(cspirv.data(), vtf::data_count(cspirv));
    outBin.close();

    return cspirv;
}

} // namespace vtf

static bool compileSpvShader (
    add_cref<std::string>           shaderCode,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder,
    add_ref<std::vector<uint32_t>>  binary)
{
    spvtools::SpirvTools tools(makeSpvTargetEnv(vulkanVer, spirvVer));
    tools.SetMessageConsumer([&](spv_message_level_t level, add_cptr<char>,
        add_cref<spv_position_t> position, add_cptr<char>message) {
            UNREF(level);
            errorCollection << "SPIR-V Assemble: " << message
                << " at line " << position.index << std::endl;
        });

    progressRecorder.stamp("Before spvtools::SpirvTools::Assemble()");
    const bool result = tools.Assemble(shaderCode, &binary);
    progressRecorder.stamp("After spvtools::SpirvTools::Assemble()");

    return result;
}

static bool validateSpirv (
    add_cref<std::vector<uint32_t>> binary,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_cref<std::string>           validationOptions,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder)
{
    spvtools::SpirvTools tools(makeSpvTargetEnv(vulkanVer, spirvVer));
    tools.SetMessageConsumer([&](spv_message_level_t level, add_cptr<char>,
        add_cref<spv_position_t> position, add_cptr<char>message) {
            UNREF(level);
            errorCollection << "SPIR-V validation: " << message
                << " at line " << position.index << std::endl;
        });

    spv_validator_options options = spvValidatorOptionsCreate();
    if (validationOptions.find("--scalar-block-layout") != std::string::npos)
    {
        spvValidatorOptionsSetScalarBlockLayout(options, true);
    }

    progressRecorder.stamp("Before spvtools::SpirvTools::Validate()");
    const bool result =  tools.Validate(binary.data(), binary.size(), options);
    progressRecorder.stamp("After spvtools::SpirvTools::Validate()");

    return result;
}

static bool generateDisassembly (
    add_cref<std::vector<uint32_t>> binary,
    add_cref<fs::path>              asmPath,
    add_cref<vtf::Version>          vulkanVer,
    add_cref<vtf::Version>          spirvVer,
    add_ref<std::vector<char>>      disassembled,
    add_ref<std::stringstream>      errorCollection,
    add_ref<vtf::ProgressRecorder>  progressRecorder)
{
    spvtools::SpirvTools tools(makeSpvTargetEnv(vulkanVer, spirvVer));
    tools.SetMessageConsumer([&](spv_message_level_t level, add_cptr<char>,
        add_cref<spv_position_t> position, add_cptr<char>message) {
            UNREF(level);
            errorCollection << "SPIR-V disassembly: " << message
                << " at line " << position.index << std::endl;
        });

    std::string disassemble;
    progressRecorder.stamp("Before spvtools::SpirvTools::Disassemble()");
    const bool result = tools.Disassemble(binary, &disassemble);
    if (false == result)
    {
        progressRecorder.stamp("spvtools::SpirvTools::Disassemble() failed");
    }
    else
    {
        progressRecorder.stamp("After spvtools::SpirvTools::Disassemble()");

        disassembled.resize(disassemble.length());
        std::copy(disassemble.begin(), disassemble.end(), disassembled.begin());

        std::ofstream outAsm(asmPath);
        outAsm.write(disassemble.data(), std::streamsize(disassemble.length()));
        outAsm.close();

    }
    return result;
}

static const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};
static add_cptr<TBuiltInResource> getDefaultTBuiltInResource ()
{
    return &DefaultTBuiltInResource;
}
