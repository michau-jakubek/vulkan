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
    uint32_t mode = 0;
    uint32_t threads = 32;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    float seed = 17.0f;
    int intField = 0;
    int floatField = 0;
    int vec2Field = 0;
    int vec3Field = 0;
    int vec4Field = 0;
    int mat2x2Field = 0;
    int mat2x3Field = 0;
    int mat2x4Field = 0;
    int mat3x2Field = 0;
    int mat3x3Field = 0;
    int mat3x4Field = 0;
    int mat4x2Field = 0;
    int mat4x3Field = 0;
    int mat4x4Field = 0;
    bool allTypeFields = false;
    strings arrays;
    strings structs;
    strings structArrays;
    bool printStructs = false;
    bool monitor = false;
    bool buildAlways = false;
    Params (add_cref<std::string> assets_) : assets(assets_) {}
    bool parse (add_ref<CommandLine> cmd, add_ref<std::ostream> str);
};

constexpr Option optSeed{ "-seed", 1 };
constexpr Option optThreads{ "-threads", 1 };
constexpr Option optIntField   { "--int",    0, 100 };
constexpr Option optFloatField { "--float",  0, 101 };
constexpr Option optVec2Field  { "--vec2",   0, 102 };
constexpr Option optVec3Field  { "--vec3",   0, 103 };
constexpr Option optVec4Field  { "--vec4",   0, 104 };
constexpr Option optMat2x2Field{ "--mat2x2", 0, 105 };
constexpr Option optMat2x3Field{ "--mat2x3", 0, 106 };
constexpr Option optMat2x4Field{ "--mat2x4", 0, 107 };
constexpr Option optMat3x2Field{ "--mat3x2", 0, 108 };
constexpr Option optMat3x3Field{ "--mat3x3", 0, 109 };
constexpr Option optMat3x4Field{ "--mat3x4", 0, 110 };
constexpr Option optMat4x2Field{ "--mat4x2", 0, 111 };
constexpr Option optMat4x3Field{ "--mat4x3", 0, 112 };
constexpr Option optMat4x4Field{ "--mat4x4", 0, 113 };
constexpr Option optAllTypeFields{ "--all-types", 0 };
constexpr Option optArray      { "-array",  99, 200 };
constexpr Option optStruct     { "-struct", 99, 300 };
constexpr Option optStructArray{ "-struct-array", 99 };
constexpr Option optPrintStructs{ "--print-structs", 0 };
constexpr Option optMonitor{ "--monitor", 0 };
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
        add_ref<OptionParserState>	state) -> int
    {
		//FromTextDispatcher<int>::fromText(text, parsed, revList, sender.m_storage, status);
            return false;
    };
    auto cbss = [&](
        add_cref<std::string>		text,
        const uint32_t              parsed,
        add_cref<strings>           revList,
        add_cref<OptionT<int>>      sender,
        add_ref<bool>				status,
        add_ref<OptionParserState>	state) -> int
    {
            return false;
    };

    OptionParser<Params> p(*this);
    OptionFlags flagsNone{ OptionFlag::None };
    OptionFlags flagsDefault{ OptionFlag::PrintDefault};

    p.addOption(&Params::intField, optIntField, "include field of int type", { intField }, flagsNone, cbui);
    p.addOption(&Params::floatField, optFloatField,
        "include field of float type; "
        "if none of the types is defined, the float type will be added automatically", { floatField }, flagsNone, cbui);
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
    p.addOption(&Params::allTypeFields, optAllTypeFields, "include all field types", { allTypeFields }, flagsNone);
    p.addOption(&Params::arrays, optArray,
        "define array(s) of built-in types you specified as comma separated type indicies, "
        "it may be present multiple times and indices start from 0", { arrays }, flagsNone)
        ->setTypeName("comma text");
    p.addOption(&Params::structs, optStruct,
        "define struct(s) of built-in types you specified or/and array(s) as comma separated entity indicies, "
        "it may be present multiple times - built-in types start from 0 and arrays from 100", { structs }, flagsNone)
        ->setTypeName("comma text");
    p.addOption(&Params::structArrays, optStructArray,
        "define array(s) of structs you specified as comma separated struct indicies, "
        "it may be present multiple times and indices start from 0", { structArrays }, flagsNone)
        ->setTypeName("comma text");
    p.addOption(&Params::printStructs, optPrintStructs, "print generated structures", { printStructs }, flagsNone);
    p.addOption(&Params::monitor, optMonitor, "print struct creation", { monitor }, flagsNone);
    p.addOption(&Params::buildAlways, optBuildAlways, "always build shader(s)", { buildAlways }, flagsNone);

    p.parse(cmd);

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

    return true;
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

        caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray)
            .checkSupported("runtimeDescriptorArray");
        caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::shaderStorageBufferArrayNonUniformIndexing)
            .checkSupported("shaderStorageBufferArrayNonUniformIndexing");
        //caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan12Features::descriptorIndexing)
        //    .checkSupported("descriptorIndexing");
        //caps.addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME).checkSupported();

        params.minStorageBufferOffsetAlignment =
            deviceGetPhysicalLimits(caps.physicalDevice).minStorageBufferOffsetAlignment;
    };

    VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 2), gf.debugPrintfEnabled);
    return runTest(ctx, params);
}

ZShaderModule genShader(ZDevice device, add_cref<Params> params,
    add_cref<std::vector<INodePtr>> list, uint32_t openArrayLength, uint32_t arrayPadCount)
{
    StructGenerator sg;
    std::ostringstream code;
    code << "#version 450 core\n";
    if (openArrayLength)
    {
        code << "#extension GL_EXT_nonuniform_qualifier : require\n";
    }
    code << "layout(local_size_x = " << (openArrayLength ? openArrayLength : 1u) << ", local_size_y = 1, local_size_z = 1) in;\n";
    code << "layout(push_constant) uniform PC {\n";
    INode::putIndent(code, 1); code << "float seed;\n";
    INode::putIndent(code, 1); code << "int visits;\n";
    code << "} pc; \n";
    for (uint32_t s = 0; s < list.size() - 1; ++s)
    {
        sg.printStruct(list[s], code, true);
    }
    code << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
    sg.printStruct(list.back(), code, false);
    code << " root";
    if (openArrayLength)
    {
        code << "[/* " << (openArrayLength + arrayPadCount * 2) << " start from " << arrayPadCount << " */]";
    }
    code << ";\n";
    code << "void main() {\n";
    INode::putIndent(code, 1);
    code << "float seed = pc.seed + gl_LocalInvocationID.x * pc.visits;\n";
    if (openArrayLength)
    {
        INode::putIndent(code, 1);
        std::ostringstream root;
        root << "root[gl_LocalInvocationID.x + " << arrayPadCount << ']';
        sg.generateLoops(list.back(), root.str(), code, 1, "seed++");
    }
    else
    {
        INode::putIndent(code, 1);
        sg.generateLoops(list.back(), "root", code, 1, "seed++");
    }
    INode::putIndent(code, 1);
    code << "}\n";

    //std::cout << code.str() << std::endl;
    auto calcNewLineCount = [](add_cref<std::string> s)
    {
        return std::count(s.begin(), s.end(), '\n');
    };
    if (params.printStructs)
    {
        std::cout << "layout(binding = 1) buffer Counter { float value[" << params.threads << "]; } counter;\n";
        for (uint32_t s = 0; s < list.size() - 1; ++s)
        {
            sg.printStruct(list[s], std::cout, true);
        }
        std::cout << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
        sg.printStruct(list.back(), std::cout, false);
        std::cout << " root" << (openArrayLength ? "[]" : "") << ";\n";
    }
    std::cout << "Shader line count: " << calcNewLineCount(code.str()) << std::endl;

    ProgramCollection		programs	(device, params.assets);
    programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, code.str());
    programs.buildAndVerify(params.buildAlways);
    return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

namespace X
{

struct TypeNode {
    int index;                      // 0–299
    TypeKind kind;
    std::vector<int> children;      // indeksy typów, które zawiera
};

inline TypeKind kindFromIndex(int idx) {
    if (idx < 100) return TypeKind::Field;
    if (idx < 200) return TypeKind::Array;
    if (idx < 300) return TypeKind::Struct;
	ASSERTFALSE("Invalid type index: " + std::to_string(idx));
	return TypeKind::Unknown;
}

TypeNode parseTypeDescription(int index, add_cref<std::string> desc) {
    TypeNode node;
    node.index = index;
    node.kind = kindFromIndex(index);

    auto parts = splitString(desc, ',');
    for (add_cref<std::string> p : parts) {
        if (p.empty()) continue;

        bool ok = false;
        int child = fromText(p, 0, ok);
        if (!ok) {
            throw std::runtime_error("Invalid type index: " + p);
        }
        node.children.push_back(child);
    }

    return node;
}

std::unordered_map<int, TypeNode>
buildGraph(add_cref<strings> descriptions)
{
    std::unordered_map<int, TypeNode> graph;

    for (int i = 0; i < descriptions.size(); ++i) {
        graph[i] = parseTypeDescription(i, descriptions[i]);
    }

    return graph;
}

void addTypeNode(add_ref<std::unordered_map<int, TypeNode>> graph,
    int index,
    const std::string& desc)
{
    graph[index] = parseTypeDescription(index, desc);
}

bool dfsCycle(int v,
    add_cref<std::unordered_map<int, TypeNode>> graph,
    add_ref<std::unordered_set<int>> visited,
    add_ref<std::unordered_set<int>> stack)
{
    if (stack.count(v)) return true;
    if (visited.count(v)) return false;

    visited.insert(v);
    stack.insert(v);

    auto it = graph.find(v);
    if (it != graph.end()) {
        for (int child : it->second.children) {
            if (dfsCycle(child, graph, visited, stack))
                return true;
        }
    }

    stack.erase(v);
    return false;
}

bool hasCycle(add_cref<const std::unordered_map<int, TypeNode>> graph) {
    std::unordered_set<int> visited, stack;
    for (auto& [idx, node] : graph) {
        if (dfsCycle(idx, graph, visited, stack))
            return true;
    }
    return false;
}

void topoDFS(int v,
    add_cref<std::unordered_map<int, TypeNode>> graph,
    add_ref<std::unordered_set<int>> visited,
    add_ref<std::vector<int>> order)
{
    if (visited.count(v)) return;
    visited.insert(v);

    auto it = graph.find(v);
    if (it != graph.end()) {
        for (int child : it->second.children) {
            topoDFS(child, graph, visited, order);
        }
    }

    order.push_back(v);
}

std::vector<int> topoSort(add_cref<std::unordered_map<int, TypeNode>> graph) {
    std::unordered_set<int> visited;
    std::vector<int> order;

    for (auto& [idx, node] : graph) {
        topoDFS(idx, graph, visited, order);
    }

    std::reverse(order.begin(), order.end());
    return order;
}

INodePtr buildType(int index,
    add_cref<std::unordered_map<int, TypeNode>> graph,
    add_ref<std::unordered_map<int, INodePtr>> built,
    add_ref<StructGenerator> sg
    )
{
    if (built.count(index))
        return built[index];

    add_cref<TypeNode> node = graph.at(index);

    std::vector<INodePtr> children;
    children.reserve(node.children.size());

    for (int childIndex : node.children) {
        children.push_back(buildType(childIndex, graph, built, sg));
    }

    INodePtr result;

    switch (node.kind) {
    case TypeKind::Field:
        result = sg.makeField(index);
        break;

    case TypeKind::Array:
        result = sg.makeArrayField(children[0], /*size*/ 5, false);
        break;

    case TypeKind::Struct:
		result = sg.generateStruct("S" + std::to_string(index), children);
        break;
    }

    built[index] = result;
    return result;
}

std::vector<INodePtr>
buildAllTypes(const std::vector<std::string>& descriptions,
    const std::vector<INodePtr>& primitiveTypes,
    StructGenerator& sg)
{
    auto graph = buildGraph(descriptions);

    if (hasCycle(graph)) {
        throw std::runtime_error("Cycle detected in type definitions");
    }

    auto order = topoSort(graph);

    std::unordered_map<int, INodePtr> built;

    // typy proste już istnieją
    for (int i = 0; i < primitiveTypes.size(); ++i) {
        built[i] = primitiveTypes[i];
    }

    // budujemy wszystkie typy
    for (int idx : order) {
        if (!built.count(idx)) {
            built[idx] = buildType(idx, graph, built, sg);
        }
    }

    // zwracamy w kolejności indeksów
    std::vector<INodePtr> result(descriptions.size());
    for (auto& [idx, node] : built) {
        if (idx < result.size())
            result[idx] = node;
    }

    return result;
}

} // namespace X

std::vector<INodePtr> generateArrays(
    add_cref<StructGenerator> sg,
    add_cref<std::vector<INodePtr>> types,
    add_cref<Params> p,
    add_ref<std::ostream> log)
{
    const uint32_t typesSize = data_count(types);
    ASSERTMSG(typesSize, "types must not be empty");
    std::vector<INodePtr> arrays;
    for (add_cref<std::string> commas : p.arrays)
    {
        const strings sTypeIndices = splitString(commas, ',');
        for (add_cref<std::string> sTypeIndex : sTypeIndices)
        {
            bool status = false;
            const int typeIndex = fromText(sTypeIndex, 0, status);
            if (status && (make_unsigned(typeIndex) < typesSize))
            {
                const uint32_t arraySize = uint32_t(std::rand()) % typesSize;
                arrays.push_back(sg.makeArrayField(types[typeIndex], (arraySize + 1u), false));
            }
            else
            {
                log << "WARNING: ";
                if (false == status)
                {
                    log << "Unable to parse built-in type index from \"" << sTypeIndex << '\"';
                }
                else
                {
                    log << "Index (" << typeIndex << ") of bounds types<0, " << typesSize << ')';
                }
                log << ", no array will be added\n";
            }
        }
    }

    return arrays;
}

INodePtr generateStructure(const float fseed, uint32_t& rootArraySize, add_cref<Params> p, add_ref<std::ostream> log)
{
    StructGenerator sg;
    const uint32_t uiseed = uint32_t(fseed);
    std::srand(uiseed);
    
    std::vector<INodePtr> types;
    if (p.allTypeFields || p.floatField ) types.push_back(sg.makeField(float()));
    if (p.allTypeFields || p.intField   ) types.push_back(sg.makeField(int()));
    if (p.allTypeFields || p.vec2Field  ) types.push_back(sg.makeField(vec<2>()));
    if (p.allTypeFields || p.vec3Field  ) types.push_back(sg.makeField(vec<3>()));
    if (p.allTypeFields || p.vec4Field  ) types.push_back(sg.makeField(vec<4>()));
    if (p.allTypeFields || p.mat2x2Field) types.push_back(sg.makeField(mat<2,2>()));
    if (p.allTypeFields || p.mat2x3Field) types.push_back(sg.makeField(mat<2,3>()));
    if (p.allTypeFields || p.mat2x4Field) types.push_back(sg.makeField(mat<2,4>()));
    if (p.allTypeFields || p.mat3x2Field) types.push_back(sg.makeField(mat<3,2>()));
    if (p.allTypeFields || p.mat3x3Field) types.push_back(sg.makeField(mat<3,3>()));
    if (p.allTypeFields || p.mat3x4Field) types.push_back(sg.makeField(mat<3,4>()));
    if (p.allTypeFields || p.mat4x2Field) types.push_back(sg.makeField(mat<4,2>()));
    if (p.allTypeFields || p.mat4x3Field) types.push_back(sg.makeField(mat<4,3>()));
    if (p.allTypeFields || p.mat4x4Field) types.push_back(sg.makeField(mat<4,4>()));

    if (types.empty())
    {
        log << "WARNING: No built-in types defined, default float is used\n";
        types.push_back(sg.makeField(float()));
    }

    const uint32_t typesSize = uint32_t(types.size());

    const std::vector<INodePtr> arrays = generateArrays(sg, types, p, log);

    std::vector<INodePtr> structures(typesSize);
    for (uint32_t i = 0u; i < typesSize; ++i)
    {
        const uint32_t startT = uint32_t(std::rand()) % (typesSize > 1 ? typesSize - 1u : typesSize);
        const uint32_t requestedT = (uint32_t(std::rand()) % typesSize) + 1u;
        const uint32_t endT = std::min(startT + requestedT, typesSize);
        const uint32_t countT = endT - startT;

        const uint32_t startA = uint32_t(std::rand()) % (typesSize > 1 ? typesSize - 1u : typesSize);
        const uint32_t requestedA = (uint32_t(std::rand()) % typesSize) + 1u;
        const uint32_t endA = std::min(startA + requestedA, typesSize);
        const uint32_t countA = endA - startA;

        std::vector<INodePtr> fields(countT + countA);
        std::copy_n(std::next(types.begin(), startT), countT, fields.begin());
        std::copy_n(std::next(arrays.begin(), startA), countA, std::next(fields.begin(), countT));

        structures[i] = sg.generateStruct("S" + std::to_string(i), fields);
    }

    std::vector<INodePtr> array_of_structures(typesSize);
    for (uint32_t i = 0u; i < typesSize; ++i)
    {
        const uint32_t arraySize = uint32_t(std::rand()) % typesSize;
        array_of_structures[i] = sg.makeArrayField(structures[i], (arraySize + 1u), false);
    }

    add_cptr<std::vector<INodePtr>> sources[]{ &array_of_structures, &structures, &arrays, &types};
    std::vector<INodePtr> all(typesSize * ARRAY_LENGTH(sources));
    for (uint32_t i = 0u; i < ARRAY_LENGTH_CAST(sources, uint32_t); ++i)
    {
        std::copy_n(sources[i]->begin(), typesSize, std::next(all.begin(), (i * typesSize)));
    }
    std::mt19937 rng(static_cast<std::mt19937::result_type>(std::rand()));
    std::shuffle(all.begin(), all.end(), rng);

    std::size_t biggestStructSize = 0u;
    uint32_t biggestStructIndex = 0u;
    for (uint32_t i = 0u; i < typesSize; ++i)
    {
        const std::size_t structSize = structures[i]->getLogicalSize();
        if (structSize > biggestStructSize)
        {
            biggestStructSize = structSize;
            biggestStructIndex = i;
        }
    }

    //INodePtr temp = generateStruct("Root", types);
    rootArraySize = ((uint32_t(std::rand()) % typesSize) + 1u) * 4u;
    INodePtr dyna = sg.generateStruct("Root", all);
    const uint32_t nestedArraySize = (uint32_t(std::rand()) % typesSize) + 1u;
    sg.structureAppendField(dyna, sg.makeArrayField(structures[biggestStructIndex], nestedArraySize, true));
    return dyna;;
}

TriLogicInt runTest (add_ref<VulkanContext> ctx, add_cref<Params> params)
{
    StructGenerator         sg;
    const float             testSeed        = params.seed;
    uint32_t                openArrayLength = 2u;
    INodePtr                gen_z           = generateStructure(testSeed, openArrayLength, params, std::cout);
    const uint32_t          visits          = gen_z->getVisitCount();
    const std::vector<INodePtr> list        = sg.getStructList(gen_z);
    const std::size_t       arrayPadCount   = 4u;
    ZShaderModule			shader          = genShader(ctx.device, params, list, openArrayLength, arrayPadCount);

    LayoutManager           lm              (ctx.device);
    const std::size_t       logicalSize     = gen_z->getLogicalSize();
    const std::size_t       arrayElemSize   = ROUNDUP(logicalSize, params.minStorageBufferOffsetAlignment);
    const std::size_t       arrayBuffSize   = (openArrayLength + (arrayPadCount * 2)) * arrayElemSize;
    ZBuffer                 arrayBuffer     = createBuffer<std::byte>(ctx.device, uint32_t(arrayBuffSize));
    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    const uint32_t          arrayBinding    = lm.addArrayBinding(arrayBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
                                                                (openArrayLength + (arrayPadCount * 2)), uint32_t(arrayElemSize), stage);
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
    auto serializeOrDeserializeMonitor = [&](add_cref<INode::SDParams> cb) -> void
    {
        UNREF(cb);
        if (false == params.monitor) return;

        auto number = [](std::size_t x) -> char
        {
            UNREF(x);
            std::cout << std::right << std::setw(6) << x << std::setw(0) << std::left;
            return ' ';
        };
        if (cb.whenAction == INode::SDAction::Serialize)
        {
            const char* action = cb.action == INode::SDAction::UpdatePadBefore ? "update pad" : "serialize";
            std::cout << std::setfill('0') << std::setw(2) << cb.fieldIndex
                << std::setfill(' ') << ") " << std::setw(10) << action << std::setw(0) << ' '
                << std::setw(18) << cb.fieldType << std::setw(0)
                << ", align: " << number(cb.alignment)
                << ", size:" << number(cb.size)
                << ", offset: " << number(cb.offset);
            if (cb.pad)
            {
                std::cout << ", pad: " << number(cb.pad);
            }
            if (watchOffset >= cb.offset && watchOffset < lastHandledOffset)
            {
                std::cout << ' ' << "#####";
            }
            std::cout << std::endl;
            lastHandledOffset = cb.offset;
        }
    };

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
            sg.serializeStruct(expected[i], expectedData.data() + offset);

            result[i] = gen_z->clone();
            sg.deserializeStruct(resultData.data(), result[i]);

            offset = offset + arrayElemSize;
        }

        auto resultDWords = reinterpret_cast<add_cptr<int32_t>>(resultData.data());
        auto expectedDWords = reinterpret_cast<add_cptr<int32_t>>(expectedData.data());
        for (uint32_t i = 0u; i < intCount; ++i)
        {
            if (resultDWords[i] != expectedDWords[i]) {
                std::cout << "DWord count per array elem is " << intCount << std::endl;
                std::cout << "Mismatch at dword " << i << std::hex;
                std::cout << ", expected 0x" << expectedDWords[i];
                std::cout << " got 0x" << resultDWords[i] << std::dec << std::endl;
                return 1;
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
