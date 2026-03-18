#include "structGeneratorTests.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZBuffer.hpp"
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

#include <unordered_map>
#include <unordered_set>

namespace
{

using namespace vtf;
using namespace sg;

struct Params
{
    add_cref<std::string> assets;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    uint32_t threads = 4;
    float seed = 17.0f;
    int floatField  = 0;
    int intField    = 0;
    int vec2Field   = 0;
    int vec3Field   = 0;
    int vec4Field   = 0;
    int mat2x2Field = 0;
    int mat2x3Field = 0;
    int mat2x4Field = 0;
    int mat3x2Field = 0;
    int mat3x3Field = 0;
    int mat3x4Field = 0;
    int mat4x2Field = 0;
    int mat4x3Field = 0;
    int mat4x4Field = 0;
    int fieldByField = -1;
    UVec4 dumpMemory{0, 0, 8, 0};
    strings arrays;
    strings structs;
    std::string openArrayType;
    std::string openArrayStruct;
    std::vector<std::pair<int, int>> arrORstrOccurences;
    bool allBuiltinFields = false;
    bool verbose = false;
    bool printStructs = false;
    bool monitor = false;
    bool buildAlways = false;
    bool sized = false;
    bool disableNonUniform = false;
    Params (add_cref<std::string> assets_) : assets(assets_) {}
    bool parse (add_ref<CommandLine> cmd, add_ref<std::ostream> str);
    void enableAllBuiltinFields ();
};

constexpr uint32_t OTT = 0;
constexpr uint32_t OTA = 100;
constexpr uint32_t OTS = 200;
constexpr uint32_t OTZ = 300;
constexpr Option optSeed{ "-seed", 1 };
constexpr Option optThreads{ "-threads", 1 };
constexpr Option optIntField   { "--int",    0, OTT };
constexpr Option optFloatField { "--float",  0, OTT };
constexpr Option optVec2Field  { "--vec2",   0, OTT };
constexpr Option optVec3Field  { "--vec3",   0, OTT };
constexpr Option optVec4Field  { "--vec4",   0, OTT };
constexpr Option optMat2x2Field{ "--mat2x2", 0, OTT };
constexpr Option optMat2x3Field{ "--mat2x3", 0, OTT };
constexpr Option optMat2x4Field{ "--mat2x4", 0, OTT };
constexpr Option optMat3x2Field{ "--mat3x2", 0, OTT };
constexpr Option optMat3x3Field{ "--mat3x3", 0, OTT };
constexpr Option optMat3x4Field{ "--mat3x4", 0, OTT };
constexpr Option optMat4x2Field{ "--mat4x2", 0, OTT };
constexpr Option optMat4x3Field{ "--mat4x3", 0, OTT };
constexpr Option optMat4x4Field{ "--mat4x4", 0, OTT };
constexpr Option optAllBuiltinFields{ "--all-types", 0 };
constexpr Option optVerbose    { "--verbose", 0 };
constexpr Option optArray      { "-a", 99, OTA };
constexpr Option optStruct     { "-s", 99, OTS };
constexpr Option optOpenArrayType{ "-open-array-type", 1 };
constexpr Option optOpenArrayStruct{ "-open-array-struct", 1 };
constexpr Option optSized{ "--sized", 0 };
constexpr Option optDisableNonUniform{ "--disable-nonuniform", 0 };
constexpr Option optPrintStructs{ "--print-structs", 0 };
constexpr Option optDumpMemory{ "-dump-memory", 1 };
constexpr Option optMonitor    { "--monitor", 0 };
constexpr Option optFieldByField{ "-field-by-field", 1 };
constexpr Option optBuildAlways{ "--build-always", 0 };
bool Params::parse (add_ref<CommandLine> cmd, add_ref<std::ostream> str)
{
    int builtinIndex = 1;
    auto cbui = [&](
        add_cref<std::string>		text,
        const uint32_t              parsed,
        add_cref<strings>           revList,
        add_ref<OptionT<int>>       sender,
        add_ref<bool>				status,
        add_ref<OptionParserState>	state) -> bool
    {
        MULTI_UNREF(text, parsed, revList, state);
        sender.m_storage = builtinIndex++;
        status = true;
        return true;
    };
    int arrORstrIndex = 0;
    auto onIterateOption = [&](add_cref<Option> opt) -> void
    {
        if (opt == optArray || opt == optStruct)
        {
            arrORstrOccurences.emplace_back(int(opt.id), arrORstrIndex++);
        }
    };
    const uint32_t threadsDefault = threads;

    OptionParser<Params> p(*this);
    OptionFlags flagsNone(OptionFlag::None);
    OptionFlags flagsDefault(OptionFlag::PrintDefault);

    p.addOption(&Params::floatField, optFloatField,
        "include field of float type; "
        "if none of the types is defined, the float type will be added automatically", { floatField }, flagsNone, cbui);
    p.addOption(&Params::intField, optIntField, "include field of int type", { intField }, flagsNone, cbui);
    p.addOption(&Params::vec2Field, optVec2Field, "include field of vec2 type", { vec2Field }, flagsNone, cbui);
    p.addOption(&Params::vec3Field, optVec3Field, "include field of vec3 type", { vec3Field }, flagsNone, cbui);
    p.addOption(&Params::vec4Field, optVec4Field, "include field of vec4 type", { vec4Field }, flagsNone, cbui);
    p.addOption(&Params::mat2x2Field, optMat2x2Field, "include field of mat2x2 type", { mat2x2Field }, flagsNone, cbui);
    p.addOption(&Params::mat2x3Field, optMat2x3Field, "include field of mat2x3 type", { mat2x3Field }, flagsNone, cbui);
    p.addOption(&Params::mat2x4Field, optMat2x4Field, "include field of mat2x4 type", { mat2x4Field }, flagsNone, cbui);
    p.addOption(&Params::mat3x2Field, optMat3x2Field, "include field of mat3x2 type", { mat3x2Field }, flagsNone, cbui);
    p.addOption(&Params::mat3x3Field, optMat3x3Field, "include field of mat3x3 type", { mat3x3Field }, flagsNone, cbui);
    p.addOption(&Params::mat3x4Field, optMat3x4Field, "include field of mat3x4 type", { mat3x4Field }, flagsNone, cbui);
    p.addOption(&Params::mat4x2Field, optMat4x2Field, "include field of mat4x2 type", { mat4x2Field }, flagsNone, cbui);
    p.addOption(&Params::mat4x3Field, optMat4x3Field, "include field of mat4x2 type", { mat4x3Field }, flagsNone, cbui);
    p.addOption(&Params::mat4x4Field, optMat4x4Field, "include field of mat4x3 type", { mat4x4Field }, flagsNone, cbui);
    p.addOption(&Params::allBuiltinFields, optAllBuiltinFields, "include all field types", { allBuiltinFields }, flagsNone);
    p.addOption(&Params::verbose, optVerbose, "enable verbose mode", { verbose }, flagsNone);
    p.addOption(&Params::arrays, optArray,
        "Define array(s) of built-in types you specified as comma separated list in format type:length. "
        "Length is optional and if not specified then 3 will be used. "
        "Tt may be present multiple times and indices start from 0", { arrays }, flagsNone)
        ->setTypeName("comma text");
    p.addOption(&Params::structs, optStruct,
        "Define struct(s) of built-in types and/or arrays and/or structs as comma separated indicies, "
        "It may be present multiple times. "
        "built-in types start from 0, arrays from 100 and structs from 200", { structs }, flagsNone)
        ->setTypeName("comma text");
    p.addOption(&Params::openArrayType, optOpenArrayType, "define an unsized array type", { openArrayType }, flagsNone);
    p.addOption(&Params::openArrayStruct, optOpenArrayStruct,
        "define a structure that will contain an unsized array", { openArrayStruct }, flagsNone);
    p.addOption(&Params::sized, optSized, "force to use sized array instead of unsized", { sized }, flagsNone);
    p.addOption(&Params::disableNonUniform, optDisableNonUniform,
        "disables nonuniformEXT in descriptor array indexing", { disableNonUniform }, flagsNone);
    p.addOption(&Params::threads, optThreads, "Define thread number. "
        "It also determines descriptor buffer array length", { threadsDefault }, flagsDefault);
    p.addOption(&Params::seed, optSeed, "set initial value for filing structures", { seed }, flagsDefault);
    p.addOption(&Params::dumpMemory, optDumpMemory, "dump result memory", { dumpMemory }, flagsDefault)
        ->setTypeName("[off[,len[,chunk[,dec]]]]");
    p.addOption(&Params::printStructs, optPrintStructs, "print generated structures", { printStructs }, flagsNone);
    p.addOption(&Params::monitor, optMonitor, "print struct creation", { monitor }, flagsNone);
    p.addOption(&Params::fieldByField, optFieldByField,
        "change checking mode to field by field using start array index, "
        "otherwise all memory is checked", { fieldByField }, flagsNone);
    p.addOption(&Params::buildAlways, optBuildAlways, "always build shader(s)", { buildAlways }, flagsNone);

    p.iterateOptions(cmd, std::bind(onIterateOption, std::placeholders::_2));
    p.parse(cmd, true, {/*onParsing*/});

    add_cref<OptionParserState> state = p.getState();
    if (state.hasHelp)
    {
        p.printOptions(str);
        return false;
    }
    if (state.hasErrors)
    {
        str << "[ERROR] " << state.messagesText() << std::endl;
        return false;
    }
    if (allBuiltinFields)
    {
        enableAllBuiltinFields();
    }
    if (threads < 2 || threads > 64)
    {
        str << "[WARNING] Number of threads should be in range <2,64>, default "
            << threadsDefault << " will be used\n";
        threads = threadsDefault;
    }

    return true;
}
void Params::enableAllBuiltinFields ()
{
    floatField  = 101;
    intField    = 102;
    vec2Field   = 103;
    vec3Field   = 104;
    vec4Field   = 105;
    mat2x2Field = 106;
    mat2x3Field = 107;
    mat2x4Field = 108;
    mat3x2Field = 109;
    mat3x3Field = 110;
    mat3x4Field = 111;
    mat4x2Field = 112;
    mat4x3Field = 113;
    mat4x4Field = 114;
}

TriLogicInt runTest(add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTest(const TestRecord& record, add_ref<CommandLine> cmdLine)
{
    UNREF(cmdLine);
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
    Params params(record.assets);
    if (false == params.parse(cmdLine, std::cout)) return {};

    auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
    {
        caps.requiredExtension.push_back("VK_KHR_shader_float16_int8");
        caps.addUpdateFeatureIf(&VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess);
        caps.addUpdateFeatureIf(&VkPhysicalDevice16BitStorageFeatures::uniformAndStorageBuffer16BitAccess);
        caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::shaderFloat16);

        if (false == params.disableNonUniform)
        {
            caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::shaderStorageBufferArrayNonUniformIndexing)
                .checkSupported("shaderStorageBufferArrayNonUniformIndexing");
        }

        caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::descriptorIndexing)
            .checkSupported("descriptorIndexing");
        caps.addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME).checkSupported();

        if (false == params.sized)
        {
            caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray)
                .checkSupported("runtimeDescriptorArray");
            caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::descriptorBindingVariableDescriptorCount)
                .checkSupported("descriptorBindingVariableDescriptorCount");
        }

        params.minStorageBufferOffsetAlignment =
            deviceGetPhysicalLimits(caps.physicalDevice).minStorageBufferOffsetAlignment;
    };

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

ZShaderModule genShader(
    ZDevice device, add_cref<Params> params,
    add_cref<std::vector<INodePtr>> list,
    uint32_t openArrayLength, uint32_t arrayPadCount,
    float seed, uint32_t visits)
{
    StructGenerator sg;
    std::ostringstream code;
    code << "#version 450 core\n";
    if (openArrayLength > 1u)
    {
        code << "#extension GL_EXT_nonuniform_qualifier : require\n";
    }
    code << "layout(local_size_x = " << (openArrayLength ? openArrayLength : 1u) << ", local_size_y = 1, local_size_z = 1) in;\n";
    code << "layout(push_constant) uniform PC {\n";
    INode::putIndent(code, 1); code << "float seed; // " << seed << '\n';
    INode::putIndent(code, 1); code << "int visits; // " << visits << '\n';
    code << "} pc; \n";
    for (uint32_t s = 0; s < list.size() - 1; ++s)
    {
        sg.printStruct(list[s], code, true);
    }
    code << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
    sg.printStruct(list.back(), code, false);
    code << " root";
    if (openArrayLength > 1u)
    {
        if (params.sized)
            code << "[/*left pad*/ " << arrayPadCount << " + "
            << openArrayLength << " + " << arrayPadCount << " /*right pad*/]";
        else
            code << "[/* " << (openArrayLength + arrayPadCount * 2) << " starts from " << arrayPadCount << " */]";
    }
    code << ";\n";
    code << "void main() {\n";
    INode::putIndent(code, 1);
    code << "float seed = pc.seed + gl_LocalInvocationID.x * pc.visits;\n";
    if (params.disableNonUniform)
    {
        code << "const uint idx = gl_LocalInvocationID.x + " << arrayPadCount << ";\n";
    }
    else
    {
        code << "const uint idx = nonuniformEXT(gl_LocalInvocationID.x + " << arrayPadCount << ");\n";
    }
    if (openArrayLength > 1u)
    {
        std::ostringstream root;
        if (params.disableNonUniform)
            root << "root[gl_LocalInvocationID.x + " << arrayPadCount << "]";
        else
            root << "root[nonuniformEXT(gl_LocalInvocationID.x + " << arrayPadCount << ")]";
        sg.generateLoops(list.back(), root.str(), code, 1, "seed++");
    }
    else
    {
        INode::putIndent(code, 1);
        sg.generateLoops(list.back(), "root", code, 1, "seed++");
    }
    code << "}\n";

    //std::cout << code.str() << std::endl;
    auto calcNewLineCount = [](add_cref<std::string> s)
    {
        return std::count(s.begin(), s.end(), '\n');
    };
    if (params.printStructs)
    {
        for (uint32_t s = 0; s < list.size() - 1; ++s)
        {
            sg.printStruct(list[s], std::cout, true);
        }
        std::cout << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
        sg.printStruct(list.back(), std::cout, false);
        if (openArrayLength > 1u)
            std::cout << " root[/* " << openArrayLength << " */];\n";
        else
            std::cout << "root;\n";
    }
    std::cout << "Shader line count: " << calcNewLineCount(code.str()) << std::endl;

    ProgramCollection		programs	(device, params.assets);
    programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, code.str());
    programs.buildAndVerify(params.buildAlways);
    return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

uint32_t generateArrays(
    add_cref<StructGenerator> sg,
    const uint32_t arrayIndex,
    add_ref<std::vector<INodePtr>> arrays,
    add_cref<std::vector<INodePtr>> types,
    add_cref<std::vector<INodePtr>> structs,
    add_cref<Params> p, add_ref<std::ostream> log,
    bool unsized)
{
    const uint32_t typesSize = data_count(types);
    const uint32_t structsSize = data_count(structs);
    if (false == unsized)
    {
        ASSERTMSG(arrayIndex < p.arrays.size(), "Array index (", arrayIndex, ") of bounds (", p.arrays.size(), ')');
    }
    const strings sTypeAndLength = unsized ? splitString(p.openArrayType, ':') : splitString(p.arrays[arrayIndex], ':');
    if (sTypeAndLength.empty() || sTypeAndLength[0].empty())
    {
        return 0;
    }

    bool status = false;
    const auto typeIndex = uint32_t(fromText(sTypeAndLength[0], -1, status));
    if (status)
    {
        INodePtr arrayType;
        if (typeIndex >= OTT && typeIndex < OTA)
        {
            if (typeIndex < typesSize)
                arrayType = types[typeIndex]->clone();
            else
            {
                log << "WARNING: Undefined built-in type " << typeIndex << " for array " << arrayIndex
                    << ", no array will be added\n";
                return 0;
            }
        }
        else if (typeIndex >= OTA && typeIndex < OTS)
        {
            log << "WARNING: Not supported array type " << typeIndex << " for array " << arrayIndex
                << ", no array will be added\n";
            return 0;
        }
        else if (typeIndex >= OTS && typeIndex < OTZ)
        {
            if ((typeIndex - OTS) < structsSize)
                arrayType = structs[typeIndex - OTS]->clone();
            else
            {
                log << "WARNING: Undefined struct type " << typeIndex << " for array " << arrayIndex
                    << ", no array will be added\n";
                return 0;
            }
        }
        else
        {
            log << "WARNING: Unknown type " << typeIndex << " for array " << arrayIndex
                << ", no array will be added\n";
            return 0;
        }

        const uint32_t arrayDefSize = 3;
        const uint32_t arrayMaxSize = 12;
        uint32_t arraySize = arrayDefSize;
        if (sTypeAndLength.size() > 1)
        {
            arraySize = fromText(sTypeAndLength[1], arrayDefSize, status);
            if (status)
            {
                if (arraySize < 1 || arraySize > arrayMaxSize)
                {
                    arraySize = arrayDefSize;
                    log << "WARNING: Array size for array " << arrayIndex
                        << " must be in range <1, " << arrayMaxSize << ">,"
                        << " default size " << arrayDefSize << " will be used\n";
                }
            }
            else
            {
                log << "WARNING: Unable to parse size for array " << arrayIndex
                    << " from \"" << sTypeAndLength[1] << "\", default size " << arrayDefSize << " will be used\n";
            }
        }

        arrays.push_back(sg.makeArrayField(arrayType, arraySize, unsized));
        if (p.verbose)
        {
            log << "Successfully created array " << arrayIndex << " of type "
                << arrays.back()->typeName << std::endl;
        }
        return arraySize;
    }
    else
    {
        log << "WARNING: Unable to parse type for array " << int(arrayIndex)
            << " from \"" << sTypeAndLength[0] << "\", no array will be added\n";
        return 0;
    }
}

uint32_t generateStructures(
    add_cref<StructGenerator> sg,
    const uint32_t structIndex,
    add_ref<std::vector<INodePtr>> structs,
    add_cref<std::vector<INodePtr>> types,
    add_cref<std::vector<INodePtr>> arrays,
    add_cref<Params> p, add_ref<std::ostream> log,
    bool unsized, const char* unsizedName)
{
    const uint32_t typesSize = data_count(types);
    if (false == unsized)
    {
        ASSERTMSG(structIndex < p.structs.size(), "Structure index (", structIndex, ") of bounds (", p.structs.size(), ')');
    }

    std::vector<INodePtr> structFields;
    add_cref<std::string> commaTypes = unsized ? p.openArrayStruct : p.structs[structIndex];
    const strings sTypes = splitString(commaTypes, ',');

    if (sTypes.empty() || sTypes[0].empty())
    {
        return 0;
    }

    auto verboseSuccess = [&](uint32_t typeIndex) {
        if (p.verbose) {
            log << "Successfully added field " << typeIndex
                << " of type " << structFields.back()->typeName << " to struct ";
            if (unsized)
                log << unsizedName;
            else
                log << structIndex;
            log << std::endl;
        }
    };

    for (add_cref<std::string> sType : sTypes)
    {
        bool status = false;
        const auto typeIndex = uint32_t(fromText(sType, -1, status));
        if (status)
        {
            if (typeIndex >= OTT && typeIndex < OTA)
            {
                if (typeIndex < typesSize)
                {
                    structFields.push_back(types[typeIndex]->clone());
                    verboseSuccess(typeIndex);
                }
                else
                {
                    log << "WARNING: Undefined built-in type " << typeIndex << " field for struct " << structIndex
                        << " from \"" << commaTypes << "\", no built-in type field will be added\n";
                    continue;
                }
            }
            else if (typeIndex >= OTA && typeIndex < OTS)
            {
                if ((typeIndex - OTA) < arrays.size())
                {
                    structFields.push_back(arrays[typeIndex - OTA]->clone());
                    verboseSuccess(typeIndex - OTA);
                }
                else
                {
                    log << "WARNING: Undefined array type " << typeIndex << " field for struct " << structIndex
                        << " from \"" << commaTypes << "\", no array type field will be added\n";
                    continue;
                }
            }
            else if (typeIndex >= OTS && typeIndex < OTZ)
            {
                if ((typeIndex - OTS) < structs.size())
                {
                    structFields.push_back(structs[typeIndex - OTS]->clone());
                    verboseSuccess(typeIndex - OTS);
                }
                else
                {
                    log << "WARNING: Undefined struct type " << typeIndex << " field for struct " << structIndex
                        << " from \"" << commaTypes << "\", no struct type field will be added\n";
                    continue;
                }
            }
            else
            {
                log << "WARNING: Unknown type " << typeIndex << " field for struct " << structIndex
                    << ", no field will be added\n";
                continue;
            }
        }
        else
        {
            log << "WARNING: Unable to parse field type from \"" << sType << '\"'
                << " for struct " << structIndex << ", no field will be added\n";
        }
    }
    if (structFields.size())
    {
        structs.push_back(sg.generateStruct((unsized ? unsizedName : "S") + std::to_string(structs.size()), structFields));
        if (p.verbose) {
            log << "Successfully created struct " << structs.back()->typeName << std::endl;
        }
    }
    else
    {
        log << "WARNING: Struct " << structIndex << " has no fields, no struct will be added\n";
    }
    return data_count(structFields);
}

std::vector<INodePtr> generateBuiltinTypes(add_cref<Params> p, add_ref<std::ostream> log)
{
    StructGenerator sg;
    std::vector<int> w;
    std::vector<INodePtr> types;
    if (p.allBuiltinFields || p.floatField ) types.push_back(sg.makeField(float()));     w.push_back(p.floatField);
    if (p.allBuiltinFields || p.intField   ) types.push_back(sg.makeField(int()));       w.push_back(p.intField);
    if (p.allBuiltinFields || p.vec2Field  ) types.push_back(sg.makeField(vec<2>()));    w.push_back(p.vec2Field);
    if (p.allBuiltinFields || p.vec3Field  ) types.push_back(sg.makeField(vec<3>()));    w.push_back(p.vec3Field);
    if (p.allBuiltinFields || p.vec4Field  ) types.push_back(sg.makeField(vec<4>()));    w.push_back(p.vec4Field);
    if (p.allBuiltinFields || p.mat2x2Field) types.push_back(sg.makeField(mat<2, 2>())); w.push_back(p.mat2x2Field);
    if (p.allBuiltinFields || p.mat2x3Field) types.push_back(sg.makeField(mat<2, 3>())); w.push_back(p.mat2x3Field);
    if (p.allBuiltinFields || p.mat2x4Field) types.push_back(sg.makeField(mat<2, 4>())); w.push_back(p.mat2x4Field);
    if (p.allBuiltinFields || p.mat3x2Field) types.push_back(sg.makeField(mat<3, 2>())); w.push_back(p.mat3x2Field);
    if (p.allBuiltinFields || p.mat3x3Field) types.push_back(sg.makeField(mat<3, 3>())); w.push_back(p.mat3x3Field);
    if (p.allBuiltinFields || p.mat3x4Field) types.push_back(sg.makeField(mat<3, 4>())); w.push_back(p.mat3x4Field);
    if (p.allBuiltinFields || p.mat4x2Field) types.push_back(sg.makeField(mat<4, 2>())); w.push_back(p.mat4x2Field);
    if (p.allBuiltinFields || p.mat4x3Field) types.push_back(sg.makeField(mat<4, 3>())); w.push_back(p.mat4x3Field);
    if (p.allBuiltinFields || p.mat4x4Field) types.push_back(sg.makeField(mat<4, 4>())); w.push_back(p.mat4x4Field);

    if (types.empty())
    {
        log << "WARNING: No built-in types defined, default float is used\n";
        types.push_back(sg.makeField(float())); w.push_back(1);
    }

    std::vector<uint32_t> ids(types.size());
    std::iota(ids.begin(), ids.end(), 0);
    std::sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) { return w[a] < w[b]; });

    std::vector<INodePtr> sortedTypes;
    sortedTypes.reserve(types.size());
    for (uint32_t i : ids)
        sortedTypes.push_back(types[i]);

    return sortedTypes;
}

INodePtr generateStructure(const float fseed, add_cref<Params> p, add_ref<std::ostream> log)
{
    StructGenerator sg;
    const uint32_t uiseed = uint32_t(fseed);
    std::srand(uiseed);
    const char* unsizedArrayStructName = "Root";

    std::vector<INodePtr> builtinTypes = generateBuiltinTypes(p, log);

    uint32_t items = 0;
    uint32_t arrayIndex = 0;
    uint32_t structIndex = 0;
    std::vector<INodePtr> arrays, structs;
    for (add_cref<std::pair<int, int>> aos : p.arrORstrOccurences)
    {
        items = 0;
        if (aos.first == int(OTA))
            items = generateArrays(sg, arrayIndex++, arrays, builtinTypes, structs, p, log, false);
        else if (aos.first == int(OTS))
            items = generateStructures(sg, structIndex++, structs, builtinTypes, arrays, p, log,
                                        false, unsizedArrayStructName);
        else ASSERTFALSE("Unrecognized type ", aos.first);
    }
    
    INodePtr unsizedArray;
    std::vector<INodePtr> tempArrays;
    add_ptr<std::vector<INodePtr>> tempArraysPtr;
    if (items = generateArrays(sg, INVALID_UINT32, arrays, builtinTypes, structs, p, log, true); items)
    {
        unsizedArray = arrays.back();
        tempArrays.assign(arrays.begin(), arrays.end() - 1);
        tempArraysPtr = &tempArrays;
    }
    else
    {
        unsizedArray = sg.makeArrayField(builtinTypes[0], 1, true);
        tempArraysPtr = &arrays;
    }

    INodePtr unsizedArrayStruct;
    if (items = generateStructures(sg, structIndex++, structs, builtinTypes, *tempArraysPtr, p, log,
        true, unsizedArrayStructName); items)
    {
        unsizedArrayStruct = structs.back();
        sg.structureAppendField(unsizedArrayStruct, unsizedArray);
    }
    else
    {
        unsizedArrayStruct = sg.generateStruct(unsizedArrayStructName, { unsizedArray });
    }

    return unsizedArrayStruct;
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    // Print current running device information
    {
        const auto p = deviceGetPhysicalProperties(ctx.device);
        printPhysicalDevice(p, std::cout);
    }

    StructGenerator         sg;
    const float             testSeed        = params.seed;
    const uint32_t          openArrayLength = params.threads;
    const uint32_t          arrayPadCount   = 4u;
    INodePtr                gen_z           = generateStructure(testSeed, params, std::cout);
    const uint32_t          visits          = gen_z->getVisitCount();
    const std::vector<INodePtr> list        = sg.getStructList(gen_z);
    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = genShader(ctx.device, params, list,
                                                        openArrayLength, arrayPadCount, testSeed, visits);

    LayoutManager           lm              (ctx.device);
    const uint32_t          arrayElemCount  = arrayPadCount + openArrayLength + arrayPadCount;
    const std::size_t       logicalSize     = gen_z->getLogicalSize();
    const std::size_t       arrayElemSize   = ROUNDUP(logicalSize, params.minStorageBufferOffsetAlignment);
    const std::size_t       arrayBuffSize   = arrayElemCount * arrayElemSize;
    ZBuffer                 arrayBuffer     = createBuffer<std::byte>(ctx.device, uint32_t(arrayBuffSize));
    const auto              bindingFlags    = params.sized
                                                ? VkDescriptorBindingFlags(0)
                                                : VkDescriptorBindingFlags(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
    const uint32_t          arrayBinding    = lm.addArrayBinding(arrayBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
                                                                arrayElemCount, uint32_t(arrayElemSize), stage, bindingFlags);
	ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
        float   seed;
        int32_t visits;
    } const                 pc              { testSeed, int32_t(visits) };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);

    {
        const auto intCount = uint32_t(arrayBuffSize / 4u);
        BufferTexelAccess<int32_t> access(arrayBuffer, uint32_t(arrayBuffSize / 4u), 1u, 1u);
        for (uint32_t i = 0u; i < intCount; ++i)
            access.at(i, 0u, 0u) = 1;
    }

	{
		OneShotCommandBuffer cmd(ctx.device, ctx.computeQueue);
		commandBufferBindPipeline(cmd, pipeline);
        commandBufferPushConstants(cmd, pipelineLayout, pc);
		commandBufferDispatch(cmd);
	}

    std::size_t watchOffset = 1824; UNREF(watchOffset);
    std::size_t lastHandledOffset = 0; UNREF(lastHandledOffset);
    PrettyPrinter pp;
    if (params.monitor)
    {
        pp.getCursor(0) << "Idx" << std::endl;
        pp.getCursor(1) << "Action" << std::endl;
        pp.getCursor(2) << "Field name" << std::endl;
        pp.getCursor(3) << "Allign" << std::endl;
        pp.getCursor(4) << "Size" << std::endl;
        pp.getCursor(5) << "Offset" << std::endl;
        pp.getCursor(6) << "Pad" << std::endl;
    }
    auto serializeOrDeserializeMonitor = [&](add_cref<INode::SDParams> cb) -> void
    {
        auto number = [](std::size_t x) -> char
        {
            std::cout << std::right << std::setw(6) << x << std::setw(0) << std::left;
            return ' ';
        };
        UNREF(number);
        if (cb.whenAction == INode::SDAction::Serialize)
        {
            const char* action = cb.action == INode::SDAction::UpdatePadBefore ? "update pad" : "serialize";
            pp.getCursor(0) << std::setfill('0') << std::setw(2) << cb.fieldIndex << std::setfill(' ') << ')' << std::endl;
            pp.getCursor(1) << action << std::endl;
            pp.getCursor(2) << cb.fieldType << std::endl;
            pp.getCursor(3) << cb.alignment << std::endl;
            pp.getCursor(4) << cb.size << std::endl;
            pp.getCursor(5) << cb.offset << std::endl;
            if (cb.pad) pp.getCursor(6) << cb.pad;
            pp.getCursor(6) << std::endl;
            /*
            if (watchOffset >= cb.offset && watchOffset < lastHandledOffset)
            {
                std::cout << ' ' << "#####";
            }
            std::cout << std::endl;
            */
            lastHandledOffset = cb.offset;
        }
    };
    INode::SDCallback sdCallback = serializeOrDeserializeMonitor;
    {
        std::vector<std::byte> resultData(lm.getBindingElementCount(arrayBinding));
        lm.readBinding(arrayBinding, resultData);
        std::vector<std::byte> expectedData(resultData.size());
        const uint32_t intCount = uint32_t(expectedData.size() / sizeof(int32_t));
        const auto expectedRange = makeStdBeginEnd<int32_t>(expectedData.data(), intCount);
        std::fill(expectedRange.first, expectedRange.second, 1);

        float seed = testSeed;
        std::size_t offset = arrayPadCount * arrayElemSize;
        std::vector<INodePtr> result(openArrayLength), expected(openArrayLength);
        for (uint32_t i = 0; i < openArrayLength; ++i)
        {
            expected[i] = gen_z->clone();
            expected[i]->loop(seed);
            sg.serializeStruct(expected[i], expectedData.data() + offset, -1,
                                (params.monitor && i == 0 ? sdCallback : nullptr));

            result[i] = gen_z->clone();
            sg.deserializeStruct(resultData.data() + offset, result[i]);

            offset = offset + arrayElemSize;
        }

        if (params.monitor)
        {
            pp.merge({ 0,1,2,3,4,5,6 }, std::cout, 2u, true);
        }

        if (params.dumpMemory[1])
        {
            /*
            const uint32_t dataSize = data_count(result);
            if (params.dumpMemory[0] < dataSize)
            {
                const uint32_t chunk = params.dumpMemory[2] < 8u ? 8u : params.dumpMemory[2];
            }
            else
            {
                std::cout << "WARNING: Offset " << params.dumpMemory[1] << " exceeds data length " << result.size()
                    << ", no data will be displayed\n";
            }
            */
            ASSERTFALSE("dumpMemory not implemented yet");
        }

        if (params.fieldByField >= 0)
        {
            uint32_t fieldByField = make_unsigned(params.fieldByField);
            if (fieldByField >= openArrayLength) {
                std::cout << "WARNING: fieldByField " << fieldByField
                    << " exceeds array length " << openArrayLength
                    << ", comparison will start from index 0" << std::endl;
                fieldByField = 0u;
            }
            std::vector<INodePtr> expectedSource, resultSource;
            for (uint32_t i = fieldByField; i < openArrayLength; ++i) {
                resultSource.push_back(result[i]);
                expectedSource.push_back(expected[i]);
            }
            INodePtr resultArray = sg.makeArrayField(resultSource, false, "root");
            INodePtr expectedArray = sg.makeArrayField(expectedSource, false, "root");
            std::string msg, expectedValue, resultValue;
            if (false == sg.compareTypes(expectedArray, resultArray, msg, expectedValue, resultValue))
            {
                std::cout << "Mismatch in field: " << msg
                    << ", expected " << expectedValue << " got " << resultValue << std::endl;
                return 1;
            }
        }
        else
        {
            auto resultDWords = reinterpret_cast<add_cptr<int32_t>>(resultData.data());
            auto expectedDWords = reinterpret_cast<add_cptr<int32_t>>(expectedData.data());
            for (uint32_t i = 0u; i < intCount; ++i)
            {
                if (resultDWords[i] != expectedDWords[i]) {
                    const auto intsInLogicalSize = logicalSize / 4u;
                    std::cout << "DWord count per array elem is " << intsInLogicalSize << std::endl;
                    std::cout << "Mismatch at dword " << i << std::hex;
                    std::cout << ", expected 0x" << expectedDWords[i];
                    std::cout << " got 0x" << resultDWords[i] << std::dec << std::endl;
                    return 1;
                }
            }
        }
	}

    return 0;
}

} // unnamed namespace

template<> struct TestRecorder<STRUCT_GENERATOR>
{
	static bool record(TestRecord&);
};
bool TestRecorder<STRUCT_GENERATOR>::record(TestRecord& record)
{
	record.name = "struct_generator";
	record.call = &prepareTest;
	return true;
}
