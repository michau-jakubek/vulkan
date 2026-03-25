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
    IVec4 dumpMemory{0, 0, 8, 0};
    strings arrays;
    strings structs;
    std::string nestedArrayType;
    std::string openArrayStruct;
    std::string printComposite;
    std::string monitor;
    std::string paramsInFile;
    std::string paramsOutFile;
    std::vector<std::pair<int, int>> arrORstrOccurences;
    int32_t fillValue = 1;
    bool allBuiltinFields = false;
    bool verbose = false;
    bool printStructs = false;
    bool buildAlways = false;
    bool sized = false;
    bool disableNonUniform = false;
    bool checkAddress = false;
    Params (add_cref<std::string> assets_) : assets(assets_) {}
    bool parse (add_ref<CommandLine> cmd, add_ref<std::ostream> str);
    void enableAllBuiltinFields ();
};

constexpr uint32_t OT0 = 100;
constexpr uint32_t OTT = OT0;
constexpr uint32_t OTA = OTT + 100;
constexpr uint32_t OTS = OTA + 100;
constexpr uint32_t OTZ = OTS + 100;
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
constexpr Option optNestedArrayType{ "-nested-array-type", 1 };
constexpr Option optOpenArrayStruct{ "-open-array-struct", 1 };
constexpr Option optSized{ "--sized", 0 };
constexpr Option optDisableNonUniform{ "--disable-nonuniform", 0 };
constexpr Option optPrintStructs{ "--print-structs", 0 };
constexpr Option optPrintComposite{ "-print-composite", 1 };
constexpr Option optDumpMemory{ "-dump-memory", 1 };
constexpr Option optMonitor    { "-monitor", 1 };
constexpr Option optCheckAddress{ "--check-address", 0 };
constexpr Option optFieldByField{ "-field-by-field", 1 };
constexpr Option optBuildAlways{ "--build-always", 0 };
constexpr Option optParamsInFile{ "-params-file-in", 1 };
constexpr Option optParamsOutFile{ "-params-file-out", 1 };
bool Params::parse (add_ref<CommandLine> cmd, add_ref<std::ostream> str)
{
    OptionParser<Params> p(*this);

    auto cbui = [&](
        add_cref<std::string>		text,
        const uint32_t              parsed,
        add_cref<strings>           revList,
        add_ref<OptionT<int>>       sender,
        add_ref<bool>				status,
        add_ref<OptionParserState>	state) -> bool
    {
        MULTI_UNREF(text, parsed, revList, sender, state);
        return (status = true);
    };
    int occurenceIndex = 1;
    auto onIterateOption = [&](add_cref<Option> opt) -> void
    {
        if (opt == optArray || opt == optStruct)
        {
            arrORstrOccurences.emplace_back(int(opt.id), occurenceIndex++);
        }
        else if (opt.id == OTT)
        {
            auto optField = p.getOptionByNameT<int>(opt);
            ASSERTMSG(optField, opt.name, " is not int");
            optField->m_storage = occurenceIndex++;
        }
    };
    const uint32_t threadsDefault = threads;

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
    p.addOption(&Params::nestedArrayType, optNestedArrayType, "define nested unsized array type", { nestedArrayType }, flagsNone);
    p.addOption(&Params::openArrayStruct, optOpenArrayStruct,
        "define a structure that will contain an unsized array", { openArrayStruct }, flagsNone);
    p.addOption(&Params::sized, optSized, "force to use sized array instead of unsized", { sized }, flagsNone);
    p.addOption(&Params::disableNonUniform, optDisableNonUniform,
        "disables nonuniformEXT in descriptor array indexing", { disableNonUniform }, flagsNone);
    p.addOption(&Params::threads, optThreads, "Define thread number. "
        "It also determines descriptor buffer array length", { threadsDefault }, flagsDefault);
    p.addOption(&Params::seed, optSeed, "set initial value for filing structures", { seed }, flagsDefault);
    p.addOption(&Params::dumpMemory, optDumpMemory,
        "dump expected and result buffer in dwords, len must be positive value to process at all,"
        " if off_in_dw is negative then starts from the begining of data", { dumpMemory }, flagsDefault)
        ->setTypeName("[off_in_dw[,len[,chunk[,dec]]]]");
    p.addOption(&Params::printStructs, optPrintStructs, "print generated structures", { printStructs }, flagsNone);
    p.addOption(&Params::printComposite, optPrintComposite,
        "print desired field accessed by composite", { printComposite }, flagsNone);
    p.addOption(&Params::monitor, optMonitor, "print array serialization", { monitor }, flagsNone)
        ->setTypeName("[index[,index[,...]]]");
    p.addOption(&Params::checkAddress, optCheckAddress, "enable address checking", { checkAddress }, flagsNone);
    p.addOption(&Params::fieldByField, optFieldByField,
        "change checking mode to field by field using start array index, "
        "otherwise all memory is checked", { fieldByField }, flagsNone);
    p.addOption(&Params::buildAlways, optBuildAlways, "always build shader(s)", { buildAlways }, flagsNone);
    p.addOption(&Params::paramsInFile, optParamsInFile,
        "file to read command line params, these params have higher priority than others", { paramsInFile }, flagsNone);
    p.addOption(&Params::paramsOutFile, optParamsOutFile, "file to write command line params", { paramsInFile }, flagsNone);

    bool paramsFromFile = false;
    {
        strings sink, paramsInOutFiles;
        int consumeFileOptionRes = (-1);
        std::vector<Option> fileOptions{ optParamsInFile, optParamsOutFile };

        consumeFileOptionRes = cmd.consumeOptions(optParamsOutFile, fileOptions, paramsInOutFiles);
        if (consumeFileOptionRes > 0) {
            std::ofstream f(paramsInOutFiles.back());
            if (f.is_open()) {
                CommandLine cmdToFile("struct_generator", cmd.getUnconsumedTokens());
                cmdToFile.consumeOptions(optParamsInFile, fileOptions, sink);
                int iToken = 0;
                for (add_cref<std::string> token : cmdToFile.getUnconsumedTokens()) {
                    if (iToken++) f << ' ';
                    f << token;
                }
                f.close();
            }
            else {
                std::cout << "WARNING: Unable to write command line params to \""
                    << paramsInOutFiles.back() << "\"\n";
            }
        }
        else if (consumeFileOptionRes < 0) {
            std::cout << "Option \"" << optParamsOutFile.name << "\" must have a value" << std::endl;
            return false;
        }

        consumeFileOptionRes = cmd.consumeOptions(optParamsInFile, fileOptions, paramsInOutFiles);
        if (consumeFileOptionRes > 0) {
            std::ifstream f(paramsInOutFiles.back());
            if (f.is_open()) {
                std::string commandLineText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                f.close();
                const strings input = splitString(commandLineText, ' ');
                CommandLine cmdFromFile("struct_generator", input);
                p.iterateOptions(cmdFromFile, std::bind(onIterateOption, std::placeholders::_2));
                p.parse(cmdFromFile, true, {/*onParsing*/ });
                paramsFromFile = true;
            }
            else {
                std::cout << "ERROR: Unable to read command line params from \""
                    << paramsInOutFiles.back() << "\"\n";
                return false;
            }
        }
        else if (consumeFileOptionRes < 0) {
            std::cout << "Option \"" << optParamsInFile.name << "\" must have a value" << std::endl;
            return false;
        }
    }

    if (false == paramsFromFile)
    {
        p.iterateOptions(cmd, std::bind(onIterateOption, std::placeholders::_2));
        p.parse(cmd, true, {/*onParsing*/ });
    }

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
    if (params.disableNonUniform)
    {
        INode::putIndent(code, 1);
        code << "float seed = pc.seed + gl_LocalInvocationID.x * pc.visits;\n";
        INode::putIndent(code, 1);
        code << "const uint idx = gl_LocalInvocationID.x + " << arrayPadCount << ";\n";
    }
    else
    {
        INode::putIndent(code, 1);
        code << "float seed = nonuniformEXT(pc.seed + gl_LocalInvocationID.x * pc.visits);\n";
        INode::putIndent(code, 1);
        code << "const uint idx = nonuniformEXT(gl_LocalInvocationID.x + " << arrayPadCount << ");\n";
    }
    if (openArrayLength > 1u)
    {
        std::ostringstream root;
        //if (params.disableNonUniform)
        //    root << "root[gl_LocalInvocationID.x + " << arrayPadCount << "]";
        //else
        //    root << "root[nonuniformEXT(gl_LocalInvocationID.x + " << arrayPadCount << ")]";
        sg.generateLoops(list.back(), "root[idx]", code, 1, "seed++");
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
            std::cout << "Visit count: " << list[s]->getVisitCount()
                << ", logical size: " << list[s]->getLogicalSize()
                << std::endl << std::endl;
        }
        std::cout << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
        sg.printStruct(list.back(), std::cout, false);
        if (openArrayLength > 1u)
            std::cout << " root[/* " << openArrayLength << " */];\n";
        else
            std::cout << "root;\n";
        std::cout << "Visit count: " << list.back()->getVisitCount()
            << ", logical size: " << list.back()->getLogicalSize() << std::endl;
        std::cout << "Shader line count: " << calcNewLineCount(code.str()) << std::endl << std::endl;
    }

    ProgramCollection		programs	(device, params.assets);
    programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, code.str());
    programs.buildAndVerify(params.buildAlways);
    return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

bool printField(
    add_cref<Params> p,
    add_ref<std::ostream> messageLog,
    INodePtr expected,
    add_ref<std::ostream> expectedPathLog,
    add_ref<std::ostream> expectedValueLog,
    INodePtr result,
    add_ref<std::ostream> resultPathLog,
    add_ref<std::ostream> resultValueLog)
{
    const strings ssPath = splitString(p.printComposite, ',');
    std::vector<uint8_t> uPath(ssPath.size());
    for (uint32_t i = 0u; i < ssPath.size(); ++i) {
        bool status = false;
        const int val = fromText(ssPath[i], -1, status);
        if (false == status) {
            messageLog << "WARNING: Unable to parse composite index " << i << " (\"" << ssPath[i]
                << "\") from \"" << p.printComposite << "\", no field will be printed\n";
            return false;
        }
        else if (val < 0 || val > 255) {
            messageLog << "WARNING: Composite index " << i << " (\"" << ssPath[i]
                << "\") from \"" << p.printComposite << "\" should be from <0,255>, no field will br printed\n";
            return false;
        }
        else {
            uPath[i] = uint8_t(val);
        }
    }
    bool rr = false;
    const bool er = StructGenerator::composite(expected, uPath, data_count(uPath), expectedPathLog, expectedValueLog);
    if (er) rr = StructGenerator::composite(result, uPath, data_count(uPath), resultPathLog, resultValueLog);

    if (false == (er && rr))
    {
        messageLog << "WARNING: Unsized array index " << uint32_t(uPath[0])
            << " from composie \"" << p.printComposite << "\" of bounds " << p.threads << ", no data will be displayed\n";
        return false;
    }
    return true;
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
    const strings sTypeAndLength = unsized ? splitString(p.nestedArrayType, ':') : splitString(p.arrays[arrayIndex], ':');
    if (sTypeAndLength.empty() || sTypeAndLength[0].empty())
    {
        return 0;
    }

    bool status = false;
    const auto typeIndex = uint32_t(fromText(sTypeAndLength[0], -1, status));
    if (status)
    {
        INodePtr arrayType;
        if (typeIndex >= (OTT - OT0) && typeIndex < (OTA - OT0))
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
        else if (typeIndex >= (OTA - OT0) && typeIndex < (OTS - OT0))
        {
            log << "WARNING: Not supported array type " << typeIndex << " for array " << arrayIndex
                << ", no array will be added\n";
            return 0;
        }
        else if (typeIndex >= (OTS - OT0) && typeIndex < (OTZ - OT0))
        {
            if ((typeIndex - (OTS - OT0)) < structsSize)
                arrayType = structs[typeIndex - (OTS - OT0)]->clone();
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
            if (typeIndex >= (OTT - OT0) && typeIndex < (OTA - OT0))
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
            else if (typeIndex >= (OTA - OT0) && typeIndex < (OTS - OT0))
            {
                const uint32_t arrayTypeIndex = typeIndex % (OTA - OT0);
                if (arrayTypeIndex < arrays.size())
                {
                    structFields.push_back(arrays[arrayTypeIndex]->clone());
                    verboseSuccess(arrayTypeIndex);
                }
                else
                {
                    log << "WARNING: Undefined array type " << typeIndex << " field for struct " << structIndex
                        << " from \"" << commaTypes << "\", no array type field will be added\n";
                    continue;
                }
            }
            else if (typeIndex >= (OTS - OT0) && typeIndex < (OTZ - OT0))
            {
                const uint32_t structTypeIndex = typeIndex % (OTS - OT0);
                if (structTypeIndex < structs.size())
                {
                    structFields.push_back(structs[structTypeIndex]->clone());
                    verboseSuccess(structTypeIndex);
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
    if (p.allBuiltinFields || p.floatField ) { types.push_back(sg.makeField(float()));     w.push_back(p.floatField);  }
    if (p.allBuiltinFields || p.intField   ) { types.push_back(sg.makeField(int()));       w.push_back(p.intField);    }
    if (p.allBuiltinFields || p.vec2Field  ) { types.push_back(sg.makeField(vec<2>()));    w.push_back(p.vec2Field);   }
    if (p.allBuiltinFields || p.vec3Field  ) { types.push_back(sg.makeField(vec<3>()));    w.push_back(p.vec3Field);   }
    if (p.allBuiltinFields || p.vec4Field  ) { types.push_back(sg.makeField(vec<4>()));    w.push_back(p.vec4Field);   }
    if (p.allBuiltinFields || p.mat2x2Field) { types.push_back(sg.makeField(mat<2, 2>())); w.push_back(p.mat2x2Field); }
    if (p.allBuiltinFields || p.mat2x3Field) { types.push_back(sg.makeField(mat<2, 3>())); w.push_back(p.mat2x3Field); }
    if (p.allBuiltinFields || p.mat2x4Field) { types.push_back(sg.makeField(mat<2, 4>())); w.push_back(p.mat2x4Field); }
    if (p.allBuiltinFields || p.mat3x2Field) { types.push_back(sg.makeField(mat<3, 2>())); w.push_back(p.mat3x2Field); }
    if (p.allBuiltinFields || p.mat3x3Field) { types.push_back(sg.makeField(mat<3, 3>())); w.push_back(p.mat3x3Field); }
    if (p.allBuiltinFields || p.mat3x4Field) { types.push_back(sg.makeField(mat<3, 4>())); w.push_back(p.mat3x4Field); }
    if (p.allBuiltinFields || p.mat4x2Field) { types.push_back(sg.makeField(mat<4, 2>())); w.push_back(p.mat4x2Field); }
    if (p.allBuiltinFields || p.mat4x3Field) { types.push_back(sg.makeField(mat<4, 3>())); w.push_back(p.mat4x3Field); }
    if (p.allBuiltinFields || p.mat4x4Field) { types.push_back(sg.makeField(mat<4, 4>())); w.push_back(p.mat4x4Field); }

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
    const uint32_t          arrayPadCount   = ROUNDUP(openArrayLength, 10);
    INodePtr                gen_z           = generateStructure(testSeed, params, std::cout);
    const uint32_t          visitCount      = gen_z->getVisitCount();
    const std::vector<INodePtr> list        = sg.getStructList(gen_z);
    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    ZShaderModule			shader          = genShader(ctx.device, params, list,
                                                        openArrayLength, arrayPadCount, testSeed, visitCount);

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
    } const                 pc              { testSeed, int32_t(visitCount) };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);

    {
        const auto intCount = uint32_t(arrayBuffSize / 4u);
        BufferTexelAccess<int32_t> access(arrayBuffer, uint32_t(arrayBuffSize / 4u), 1u, 1u);
        for (uint32_t i = 0u; i < intCount; ++i)
            access.at(i, 0u, 0u) = params.fillValue;
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
    std::vector<uint32_t> monitorIndices;
    if (params.monitor.length())
    {
        std::set<uint32_t> miSet;
        strings miStrings = splitString(params.monitor, ',');
        for (add_cref<std::string> miString : miStrings)
        {
            bool miStatus = false;
            const uint32_t miValue = fromText(miString, 0u, miStatus);
            if (miStatus) miSet.insert(miValue);
        }
        if (miSet.empty())
            monitorIndices.push_back(0u);
        else for (auto miValue : miSet)
            monitorIndices.push_back(miValue);

        pp.getCursor(0) << "Idx" << std::endl;
        pp.getCursor(1) << "Action" << std::endl;
        pp.getCursor(2) << "Field name" << std::endl;
        pp.getCursor(3) << "Allign" << std::endl;
        pp.getCursor(4) << "Size" << std::endl;
        pp.getCursor(5) << "Offset" << std::endl;
        pp.getCursor(6) << "Pad" << std::endl;
        pp.getCursor(7) << "Stride" << std::endl;
        pp.getCursor(8) << "InArray" << std::endl;
    }
    auto serializeOrDeserializeMonitor = [&](add_cref<INode::SDParams> cb) -> void
    {
        auto number = [](std::size_t x) -> char
        {
            std::cout << std::right << std::setw(6) << x << std::setw(0) << std::left;
            return ' ';
        };
        UNREF(number);
        if (cb.whenAction == INode::SDAction::Serialize
            || cb.whenAction == INode::SDAction::SerializeArray
            || cb.whenAction == INode::SDAction::SerializeStruct
            || cb.whenAction == INode::SDAction::SerializeVector
            || cb.whenAction == INode::SDAction::SerializeMatrix)
        {
            const char* action =
                cb.action == INode::SDAction::UpdatePadBefore || cb.action == INode::SDAction::UpdatePadAfter
                ? "update pad"
                : cb.whenAction == INode::SDAction::SerializeArray
                    ? "serialize[]"
                    : cb.whenAction == INode::SDAction::SerializeStruct
                        ? "serialize{}"
                        : cb.whenAction == INode::SDAction::SerializeMatrix
                            ? "serializeMat"
                            : cb.whenAction == INode::SDAction::DeserializeVector
                                ? "serializeVec"
                                : "serialize";
            pp.getCursor(0) << std::setfill('0') << std::setw(2) << cb.fieldIndex << std::setfill(' ') << ')' << std::endl;
            pp.getCursor(1) << action << std::endl;
            pp.getCursor(2) << cb.fieldType << std::endl;
            pp.getCursor(3) << cb.alignment << std::endl;
            pp.getCursor(4) << cb.size << std::endl;
            pp.getCursor(5) << cb.offset << std::endl;
            if (cb.pad) pp.getCursor(6) << cb.pad;
            pp.getCursor(6) << std::endl;
            if (cb.stride) pp.getCursor(7) << cb.stride;
            pp.getCursor(7) << std::endl;
            if (cb.inArray) pp.getCursor(8) << " * ";
            pp.getCursor(8) << std::endl;
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
    auto logPadding = [](size_t stride, size_t pad, size_t offset, uint32_t index, INode::SDCallback cb)
    {
        if (pad == 0 || cb == nullptr) return;
        INode::SDParams sdp{};
        sdp.whenAction  = INode::SDAction::SerializeStruct;
        sdp.action      = INode::SDAction::UpdatePadAfter;
        sdp.fieldIndex  = index;
        sdp.fieldType   = "root[" + std::to_string(index) + "]";
        sdp.offset      = offset - pad;
        sdp.stride      = stride;
        sdp.alignment   = stride;
        sdp.pad         = pad;
        cb(sdp);
    };
    {
        std::vector<std::byte> resultData(lm.getBindingElementCount(arrayBinding));
        lm.readBinding(arrayBinding, resultData);
        std::vector<std::byte> expectedData(resultData.size());
        const uint32_t intCount = uint32_t(expectedData.size() / sizeof(int32_t));
        const auto expectedRange = makeStdBeginEnd<int32_t>(expectedData.data(), intCount);
        std::fill(expectedRange.first, expectedRange.second, params.fillValue);

        const std::string sep1(3, '=');
        const std::string sep2(3, '-');
        float seed = testSeed;
        std::size_t serializeOffset = 0u;
        const std::size_t padSize = arrayPadCount * arrayElemSize;
        std::size_t deserializeOffset = padSize;
        std::vector<INodePtr> result(openArrayLength), expected(openArrayLength);
        for (uint32_t i = 0; i < openArrayLength; ++i)
        {
            INode::SDCallback sdcb = nullptr;
            if (monitorIndices.end() != std::find(monitorIndices.begin(), monitorIndices.end(), i))
            {
                pp.getCursor(0) << sep1 << std::endl; pp.getCursor(0) << std::endl; pp.getCursor(0) << sep2 << std::endl;// idx
                pp.getCursor(1) << sep1 << std::endl; pp.getCursor(1) << "Serialize" << std::endl; pp.getCursor(1) << sep2 << std::endl; // action
                pp.getCursor(2) << sep1 << std::endl; pp.getCursor(2) << "Element " << i << std::endl; pp.getCursor(2) << sep2 << std::endl;// field anme
                pp.getCursor(3) << sep1 << std::endl; pp.getCursor(3) << std::endl; pp.getCursor(3) << sep2 << std::endl; // alignment
                pp.getCursor(4) << sep1 << std::endl; pp.getCursor(4) << std::endl; pp.getCursor(4) << sep2 << std::endl; // size
                pp.getCursor(5) << sep1 << std::endl; pp.getCursor(5) << std::endl; pp.getCursor(5) << sep2 << std::endl; // offset
                pp.getCursor(6) << sep1 << std::endl; pp.getCursor(6) << std::endl; pp.getCursor(6) << sep2 << std::endl; // pad
                pp.getCursor(7) << sep1 << std::endl; pp.getCursor(7) << std::endl; pp.getCursor(7) << sep2 << std::endl; // stride
                pp.getCursor(8) << sep1 << std::endl; pp.getCursor(8) << std::endl; pp.getCursor(8) << sep2 << std::endl; // inArray

                sdcb = serializeOrDeserializeMonitor;
            }

            expected[i] = gen_z->clone();
            expected[i]->loop(seed);
            sg::INode::Control controlSerialize(
                expectedData.data() + padSize,
                params.checkAddress ? (expectedData.data() + padSize + arrayElemSize) : nullptr);
            sg.serializeStructWithOffset(expected[i], expectedData.data() + padSize, serializeOffset,
                                            -1, sdcb, controlSerialize);
            const size_t pad = sg::Node<sg::INodePtr>::updateOffsetWithPadding(nullptr, arrayElemSize, serializeOffset);
            logPadding(arrayElemSize, pad, serializeOffset, i, sdcb);

            result[i] = gen_z->clone();
            sg::INode::Control controlDeserialize(nullptr, nullptr);
            sg.deserializeStruct(resultData.data() + deserializeOffset, result[i], true);

            deserializeOffset = deserializeOffset + arrayElemSize;
        }

        if (params.monitor.length())
        {
            pp.merge({ 0,1,2,5,3,4,6,7,8 }, std::cout, 2u, true);
            std::cout << std::endl;
        }

        auto tryFloatOrInt = [&](uint32_t value) -> float
        {
            const float asFloat = *reinterpret_cast<add_ptr<float>>(&value);
            if ((asFloat >= testSeed) && (asFloat < ((testSeed + float(visitCount)) * float(openArrayLength))))
                return asFloat;
            return static_cast<float>(value);
        };

        // -dump-memory <[off_in_dw[,len[,chunk[,dec]]]]>
        if (params.dumpMemory[1])
        {
            const auto baseAlignmentDwordCount = arrayElemSize / 4u;
            const auto firstArrayElemOffsetDWord = arrayPadCount * baseAlignmentDwordCount;
            size_t offset = params.dumpMemory[0] < 0 ? firstArrayElemOffsetDWord : size_t(params.dumpMemory[0]);
            if (offset < intCount)
            {
                const size_t chunkSize =
                    params.dumpMemory[2] < 4 || params.dumpMemory[2] > 16 ? 16u : size_t(params.dumpMemory[2]);
                const size_t availableCount = intCount - offset;
                const size_t dumpCount = std::min(size_t(params.dumpMemory[1]), availableCount);

                auto dumpData = [&](add_cptr<std::byte> dataPtr, add_ref<std::ostream> out) -> void
                {
                    PrettyPrinter dumpPrinter;
                    auto expectedDwordPtr = reinterpret_cast<add_cptr<uint32_t>>(dataPtr + offset * 4u);
                    for (size_t x = 0, z = 0; x < dumpCount; x += chunkSize) {
                        dumpPrinter.getCursor(0) << (offset + x) << '/' << (z * 4) << ')' << std::endl;
                        for (size_t y = 0; y < chunkSize && z < dumpCount; ++y, ++z) {
                            dumpPrinter.getCursor(uint32_t(y + 1))
                                << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                                << tryFloatOrInt(expectedDwordPtr[x + y]) << std::endl;
                        }
                    }
                    dumpPrinter.merge(out, 2, true);
                    out << std::endl;
                };

                std::cout << "\nDump expected data in dwords "
                    << "- start: " << offset
                    << ", count: " << dumpCount
                    << ", available: " << availableCount << std::endl;
                dumpData(expectedData.data(), std::cout);

                std::cout << "\nDump result data in dwords "
                    << "- start: " << offset
                    << ", count: " << dumpCount
                    << ", available: " << availableCount << std::endl;
                dumpData(resultData.data(), std::cout);
            }
            else
            {
                std::cout << "\nWARNING: Offset " << offset
                    << " exceeds data dword length " << intCount << ", no data will be displayed\n";
            }
        }

        if (params.printComposite.size())
        {
            INodePtr resultArray = sg.makeArrayField(result, false, "root");
            INodePtr expectedArray = sg.makeArrayField(expected, false, "root");
            std::stringstream messageLog;
            std::stringstream expectedPathLog, expectedValueLog;
            std::stringstream resultPathLog, resultValueLog;
            const bool r = printField(params, messageLog,
                                        expectedArray, expectedPathLog, expectedValueLog,
                                        resultArray, resultPathLog, resultValueLog);

            std::cout << "Composite field access from " << params.printComposite << std::endl;
            std::cout << "Expected: ";
            std::copy(std::istreambuf_iterator<char>(expectedPathLog), std::istreambuf_iterator<char>(),
                        std::ostreambuf_iterator<char>(std::cout));
            std::cout << " = ";
            std::copy(std::istreambuf_iterator<char>(expectedValueLog), std::istreambuf_iterator<char>(),
                        std::ostreambuf_iterator<char>(std::cout));
            std::cout << std::endl;

            std::cout << "Result:   ";
            std::copy(std::istreambuf_iterator<char>(resultPathLog), std::istreambuf_iterator<char>(),
                        std::ostreambuf_iterator<char>(std::cout));
            std::cout << " = ";
            std::copy(std::istreambuf_iterator<char>(resultValueLog), std::istreambuf_iterator<char>(),
                        std::ostreambuf_iterator<char>(std::cout));
            std::cout << std::endl;

            if (r == false) {
                std::copy(std::istreambuf_iterator<char>(messageLog), std::istreambuf_iterator<char>(),
                            std::ostreambuf_iterator<char>(std::cout));
                std::cout << std::endl;
            }
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
            }
        }
        // Always do dword by dword comparison
        {
            std::cout << "\nStatistics:\n";
            const auto logicalSizeDwordCount = logicalSize / 4u;
            const auto baseAlignmentDwordCount = arrayElemSize / 4u;
            const auto firstArrayElemOffsetDWord = arrayPadCount * baseAlignmentDwordCount;
            std::cout << "DWord count per array elem is " << logicalSizeDwordCount << std::endl;
            std::cout << "DWord count per storage buffer alignment is " << (params.minStorageBufferOffsetAlignment / 4u) << std::endl;
            std::cout << "Dword count per array elem alignment is " << baseAlignmentDwordCount << std::endl;
            std::cout << "Array starts from dword " << firstArrayElemOffsetDWord << std::endl << std::endl;

            auto resultDWords = reinterpret_cast<add_cptr<uint32_t>>(resultData.data());
            auto expectedDWords = reinterpret_cast<add_cptr<uint32_t>>(expectedData.data());
            for (uint32_t i = 0u; i < intCount; ++i)
            {
                if (resultDWords[i] != expectedDWords[i]) {
                    std::cout << "Mismatch at dword " << i;
                    std::cout << ", expected hex: 0x" << std::hex << expectedDWords[i] << std::dec
                        << " float: " << tryFloatOrInt(expectedDWords[i]);
                    std::cout << ", got hex: 0x" << std::hex << resultDWords[i] << std::dec
                        << " float: " << tryFloatOrInt(resultDWords[i]) <<  std::endl;
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
