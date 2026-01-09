#include "structGeneratorTests.hpp"
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
    uint32_t mode = 0;
    uint32_t threads = 32;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    float seed = 17.0f;
    Params(add_cref<std::string> assets_) : assets(assets_) {}
};

TriLogicInt runTest(add_ref<VulkanContext> ctx, add_cref<Params> params);

TriLogicInt prepareTest(const TestRecord& record, add_ref<CommandLine> cmdLine)
{
    UNREF(cmdLine);
    add_cref<GlobalAppFlags> gf(getGlobalAppFlags());
    Params params(record.assets);

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
    add_cref<std::vector<INodePtr>> list, uint32_t openSize)
{
    StructGenerator sg;
    std::ostringstream code;
    code << "#version 450 core\n";
    if (openSize)
    {
        code << "#extension GL_EXT_nonuniform_qualifier : require\n";
    }
    code << "layout(local_size_x = " << (openSize ? openSize : 1u) << ", local_size_y = 1, local_size_z = 1) in;\n";
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
    if (openSize)
    {
        code << "[/* " << openSize << " */]";
    }
    code << ";\n";
    code << "void main() {\n";
    INode::putIndent(code, 1);
    code << "float seed = pc.seed + gl_LocalInvocationID.x * pc.visits;\n";
    if (openSize)
    {
        INode::putIndent(code, 1);
        code << "for (uint root_0 = 0; root_0 < " << openSize << "; ++root_0) {\n";
        INode::putIndent(code, 2);
        code << "if (root_0 == gl_LocalInvocationID.x) {\n";
        sg.generateLoops(list.back(), "root[nonuniformEXT(root_0)]", code, 3, "seed++");
        INode::putIndent(code, 2);
        code << "}\n";
    }
    else
    {
        sg.generateLoops(list.back(), "root", code, 2, "seed++");
    }
    INode::putIndent(code, 1);
    code << "}\n";
    code << "}\n";

    //std::cout << code.str() << std::endl;
    auto calcNewLineCount = [](add_cref<std::string> s)
    {
        return std::count(s.begin(), s.end(), '\n');
    };
    // std::cout << "layout(binding = 1) buffer Counter { float value[" << params.threads << "]; } counter;\n";
    // for (uint32_t s = 0; s < list.size() - 1; ++s)
    // {
    //     printStruct(list[s], std::cout, true);
    // }
    // std::cout << "layout(std430, binding = 0) buffer " << list.back()->typeName << ' ';
    // printStruct(list.back(), std::cout, false);
    // std::cout << " root" << (openSize ? "[]" : "") << ";\n";
    std::cout << "Shader line count: " << calcNewLineCount(code.str()) << std::endl;

    ProgramCollection		programs	(device, params.assets);
    programs.addFromText(VK_SHADER_STAGE_COMPUTE_BIT, code.str());
    programs.buildAndVerify(true);
    return programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
}

INodePtr generateStructure(const float fseed, uint32_t& rootArraySize);
INodePtr generateStructure(const float fseed, uint32_t& rootArraySize)
{
    StructGenerator sg;
    const uint32_t uiseed = uint32_t(fseed);
    std::srand(uiseed);

    std::vector<INodePtr> types{
        sg.makeField(int()), sg.makeField(float()), sg.makeField(vec<2>()), sg.makeField(vec<3>()), sg.makeField(vec<4>()),
        sg.makeField(mat<2,2>()), sg.makeField(mat<2,3>()), sg.makeField(mat<2,4>()),
        sg.makeField(mat<3,2>()), sg.makeField(mat<3,3>()), sg.makeField(mat<3,4>()), sg.makeField(mat<4,4>()) };
    const uint32_t typesSize = uint32_t(types.size());

    std::vector<INodePtr> arrays(typesSize);
    for (uint32_t i = 0u; i < typesSize; ++i)
    {
        const uint32_t arraySize = uint32_t(std::rand()) % typesSize;
        arrays[i] = sg.makeArrayField(types[i], (arraySize + 1u), false);
    }

    std::vector<INodePtr> structures(typesSize);
    for (uint32_t i = 0u; i < typesSize; ++i)
    {
        const uint32_t startT = uint32_t(std::rand()) % (typesSize - 1u);
        const uint32_t requestedT = (uint32_t(std::rand()) % typesSize) + 1u;
        const uint32_t endT = std::min(startT + requestedT, typesSize);
        const uint32_t countT = endT - startT;

        const uint32_t startA = uint32_t(std::rand()) % (typesSize - 1u);
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

    std::vector<INodePtr>* sources[]{ &array_of_structures, &structures, &arrays, &types};
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
    INodePtr                gen_z           = generateStructure(testSeed, openArrayLength);
    const uint32_t          visits          = gen_z->getVisitCount();
    const std::vector<INodePtr> list        = sg.getStructList(gen_z);
    ZShaderModule			shader          = genShader(ctx.device, params, list, openArrayLength);

    LayoutManager           lm              (ctx.device);
    const std::size_t       logicalSize     = gen_z->getLogicalSize();
    const std::size_t       structSize      = ROUNDUP(logicalSize, params.minStorageBufferOffsetAlignment);
    const std::size_t       structBuffSize  = openArrayLength * (structSize + 111);
    ZBuffer                 structBuff      = createBuffer<std::byte>(ctx.device, uint32_t(structBuffSize));
    // ZBuffer              counterBuff     = createBuffer<float>(ctx.device, params.threads);
    const VkShaderStageFlags stage          = VK_SHADER_STAGE_COMPUTE_BIT;
    const uint32_t          structBind      = lm.addArrayBinding(structBuff, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
                                                                openArrayLength, uint32_t(structSize), stage);
    // const uint32_t       counterBind     = lm.addBinding(counterBuff, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
	ZDescriptorSetLayout	dsLayout        = lm.createDescriptorSetLayout();
    struct PC {
        float   seed;
        int32_t visits;
    } const                 pc              { testSeed, int32_t(visits) };
    ZPipelineLayout			pipelineLayout  = lm.createPipelineLayout({ dsLayout }, ZPushRange<PC>(stage));
    ZPipeline				pipeline        = createComputePipeline(pipelineLayout, shader);

    /*
    {
        float seed = 3.0f;
        gen_z->init(seed);
        std::vector<std::byte> raw(lm.getBindingElementCount(structBind));
        serialize(gen_z, raw.data());
        lm.writeBinding(structBind, raw);
    }
    {
        std::vector<int> counterData(lm.getBindingElementCount(counterBind));
        std::iota(counterData.begin(), counterData.end(), int(testSeed));
        lm.writeBinding(counterBind, counterData);
    }
    */

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
        /*
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
        */
    };

    bool verdict = false;
	{
        std::vector<std::byte> binary(lm.getBindingElementCount(structBind));
        lm.readBinding(structBind, binary);
        std::vector<INodePtr> clones(openArrayLength);
        std::size_t cloneOffset = 0;
        std::vector<std::string> results(openArrayLength);
        for (uint32_t i = 0; i < openArrayLength; ++i)
        {
            clones[i] = gen_z->clone();
            sg.deserializeStruct(binary.data() + cloneOffset, clones[i]);
            results[i] = sg.getValuesString(clones[i]);
            cloneOffset += structSize;
        }
        //std::cout << "Deserialized shader output:\n";
        //printValues(clones[0], std::cout);
        //std::cout << std::endl;

        float seed = testSeed;
        std::vector<INodePtr> simulates(openArrayLength);
        std::vector<short> host(gen_z->getLogicalSize() * openArrayLength);
        std::size_t hostOffset = 0u;
        std::vector<std::string> expected(openArrayLength);
        for (uint32_t i = 0; i < openArrayLength; ++i)
        {
            simulates[i] = gen_z->clone();
            simulates[i]->loop(seed);
            if (0u == i)
            {
                sg.serializeStruct(simulates[i], reinterpret_cast<std::byte*>(host.data()) + hostOffset,
                                -1, serializeOrDeserializeMonitor);
            }
            else
            {
                sg.serializeStruct(simulates[i], reinterpret_cast<std::byte*>(host.data()) + hostOffset);
            }
            hostOffset += structSize;
            expected[i] = sg.getValuesString(simulates[i]);
        }
        //std::cout << "Expected:\n";
        //printValues(simulates[0], std::cout);
        //std::cout << std::endl;

        /*
        std::vector<float> counterData(lm.getBindingElementCount(counterBind));
        lm.readBinding(counterBind, counterData);
        std::cout << "getVisitCount() := " << gen_z->getVisitCount() << std::endl;
        std::cout << "Counter: " << counterData[0] << ", diff: " << (counterData[0] - testSeed) << std::endl;
        */

        verdict = results == expected;

        auto cast = [](float x, float xMin, float xMax) -> float
        {
            const int y = *(int*)&x;
            if (float(y) >= xMin && float(y) <= xMax)
                return float(y);
            return x;
        };

        auto printRaw = [&](add_cptr<float> p, std::size_t elemCount, std::ostream& stream, std::size_t chunkSize = 8)
        {
            for (std::size_t i = 0; i < elemCount; i += chunkSize)
            {
                stream << std::right << std::setw(6) << (i * 4u) << std::left << std::setw(0) << ") ";
                const std::size_t chunkEnd = std::min(i + chunkSize, elemCount);
                for (std::size_t j = i; j < chunkEnd; ++j)
                {
                    const float v = cast(p[j], testSeed, seed);
                    stream << std::setw(3) << std::right << v << ' ' << std::left << std::setw(0);
                }
                stream << std::endl;
            }
        }; UNREF(printRaw);

        auto compareRaw = [&](add_cptr<float> lhs, add_cptr<float> rhs, std::size_t elems)
        {
            static_cast<void>(elems);
            for (uint32_t i = 0u; i < elems; ++i)
            {
                const int a = int(cast(lhs[i], testSeed, seed));
                const int b = int(cast(rhs[i], testSeed, seed));
                if (a != b)
                {
                    return i;
                }
            }
            return uint32_t(elems);
        };

        const uint32_t dwordCount = uint32_t(clones[0]->getLogicalSize() / 4);

        /*
        PrettyPrinter pp;

        pp.getCursor(0) << "Result from shader:\n";
        printRaw(reinterpret_cast<add_cptr<float>>(binary.data()), dwordCount, pp.getCursor(0));

        pp.getCursor(1) << "Serialization from host:\n";
        printRaw(reinterpret_cast<add_cptr<float>>(host.data()), dwordCount, pp.getCursor(1));

        pp.merge({ 0, 1 }, std::cout);
        */

        const uint32_t cmp = compareRaw(reinterpret_cast<add_cptr<float>>(binary.data()),
                                    reinterpret_cast<add_cptr<float>>(host.data()), dwordCount);
        if (cmp != dwordCount)
        {
            std::cout << "First difference at dword: " << cmp << ", byte: " << (cmp * 4) << std::endl;
        }
	}

    return verdict ? 0 : 1;
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
