#include "suzanneGltf.hpp"
#include "vtfZDeletable.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfVertexInput.hpp"
#include "vtfCommandLine.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfBacktrace.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZRenderPass2.hpp"
#include "vtfMatrix.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
// tiny_gltf.h needs to be included after the implementation define, otherwise it will not compile.
// This is because the implementation define is used to control the inclusion of the implementation
// code in tiny_gltf.h. If tiny_gltf.h is included before the implementation define, the implementation
// code will not be included and the functions defined in tiny_gltf.h will not be available for use.
#include "tiny_gltf.h"

namespace
{
using namespace vtf;

struct Params
{
    add_cref<std::string> assets;
    Params(add_cref<std::string> assets_)
        : assets(assets_) {}
    std::string meshFilename;
    OptionParser<Params> getParser();
};
constexpr Option optionMeshFilename{ "-mesh-file", 1 };
auto Params::getParser() -> OptionParser<Params>
{
    OptionParser<Params> parser(*this);
    parser.addOption(&Params::meshFilename, optionMeshFilename, "Path to the glTF mesh file (e.g., suzanne.glb)");
    return parser;
}

TriLogicInt runTest(add_ref<Canvas> ctx, add_cref<Params> params);

TriLogicInt prepareTest(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
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

    CanvasStyle canvasStyle = Canvas::DefaultStyle;
    canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
    Canvas ctx(record.name, gf.layers, strings(), strings(), canvasStyle,
        {}, Version(1, 4), gf.debugPrintfEnabled);

    return runTest(ctx, params);
}

void loadGltfMesh(
    add_cref<std::string> filename,
    add_ref<std::vector<Vec3>> outPositions,
	add_ref<std::vector<Vec3>> outNormals,
	add_ref<std::vector<Vec2>> outUVs,
    add_ref<std::vector<uint32_t>> outIndices) 
{
    UNREF(outUVs);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = false;
    if (filename.substr(filename.find_last_of(".") + 1) == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    }
    else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    }

    ASSERTMSG(warn.empty(), "Warning while loading glTF file: ", filename, "\nWarning: ", warn);
    ASSERTMSG(ret, "Failed to load glTF file: ", filename, "\nError: ", err,
		    (warn.empty() ? "" : "\nWarning: "), warn);

    for (add_cref<tinygltf::Mesh> mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives)
	{
            if (primitive.mode != 4) continue;

            const auto& accessorPos = model.accessors[make_unsigned(primitive.attributes.at("POSITION"))];
            const auto& bufferViewPos = model.bufferViews[make_unsigned(accessorPos.bufferView)];
            const auto& bufferPos = model.buffers[make_unsigned(bufferViewPos.buffer)];
            const float* positions = reinterpret_cast<const float*>(&bufferPos.data[bufferViewPos.byteOffset + accessorPos.byteOffset]);

            const float* normals = nullptr;
            add_cptr<tinygltf::Accessor> accessorNormPtr = nullptr;
            if (primitive.attributes.count("NORMAL")) {
                const auto& accessorNorm = model.accessors[make_unsigned(primitive.attributes.at("NORMAL"))];
                const auto& bufferViewNorm = model.bufferViews[make_unsigned(accessorNorm.bufferView)];
                const auto& bufferNorm = model.buffers[make_unsigned(bufferViewNorm.buffer)];
                normals = reinterpret_cast<const float*>(&bufferNorm.data[bufferViewNorm.byteOffset + accessorNorm.byteOffset]);
				accessorNormPtr = &accessorNorm;
            }

            for (size_t i = 0; i < accessorPos.count; ++i) {
                outPositions.emplace_back();
				add_ref<Vec3> v = outPositions.back();
                v[0] = positions[i * 3 + 0];
                v[1] = positions[i * 3 + 1];
                v[2] = positions[i * 3 + 2];
            }

            if (normals) {
                for (size_t i = 0; i < accessorNormPtr->count; ++i) {
                    outNormals.emplace_back();
                    add_ref<Vec3> n = outNormals.back();
                    n[0] = normals[i * 3 + 0];
                    n[1] = normals[i * 3 + 1];
                    n[2] = normals[i * 3 + 2];
                }
            }

            const auto& accessorIdx = model.accessors[make_unsigned(primitive.indices)];
            const auto& bufferViewIdx = model.bufferViews[make_unsigned(accessorIdx.bufferView)];
            const auto& bufferIdx = model.buffers[make_unsigned(bufferViewIdx.buffer)];
            const void* indexData = &bufferIdx.data[bufferViewIdx.byteOffset + accessorIdx.byteOffset];

            if (accessorIdx.componentType == 5123) { // unsigned short (uint16_t)
                const uint16_t* idx = static_cast<const uint16_t*>(indexData);
                for (size_t i = 0; i < accessorIdx.count; ++i) {
                    outIndices.push_back(static_cast<uint32_t>(idx[i]));
                }
            }
            else if (accessorIdx.componentType == 5125) { // unsigned int (uint32_t)
                const uint32_t* idx = static_cast<const uint32_t*>(indexData);
                for (size_t i = 0; i < accessorIdx.count; ++i) {
                    outIndices.push_back(idx[i]);
                }
            }
        }
    }
}

void calcMinMaxPosition(add_cref<std::vector<Vec3>> positions, add_ref<Vec3> minPosition, add_ref<Vec3> maxPosition)
{
    float minX = 1e10, maxX = -1e10;
    float minY = 1e10, maxY = -1e10;
    float minZ = 1e10, maxZ = -1e10;
    for (add_cref<Vec3> v : positions) {
        if (v.x() < minX) minX = v.x();
        if (v.x() > maxX) maxX = v.x();
        if (v.y() < minY) minY = v.y();
        if (v.y() > maxY) maxY = v.y();
        if (v.z() < minZ) minZ = v.z();
        if (v.z() > maxZ) maxZ = v.z();
    }
    minPosition.x(minX); maxPosition.x(maxX);
    minPosition.y(minY); maxPosition.y(maxY);
    minPosition.z(minZ); maxPosition.z(maxZ);
}

[[maybe_unused]] std::pair<ZBuffer, uint32_t> prepareTestMesh(
    add_ref<VertexInput> vi, add_ref<Vec3> minPosition, add_ref<Vec3> maxPosition)
{
	std::vector<Vec3> positions{
        { -0.5f, -0.5f, 0.0f },
        {  0.5f, -0.5f, 0.0f },
        {  0.0f,  0.5f, 0.0f }
	};
    std::vector<Vec3> normals{
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f }
	};
	std::vector<uint32_t> indices{ 0, 1, 2 };
	ZBuffer indexBuffer = createIndexBuffer(vi.device, data_count(indices), VK_INDEX_TYPE_UINT32);
	bufferWrite(indexBuffer, indices);
	vi.binding(0).addAttributes(positions, normals);
	calcMinMaxPosition(positions, minPosition, maxPosition);
	return { indexBuffer, data_count(indices) };
}

std::pair<ZBuffer, uint32_t> prepareMesh(add_cref<std::string> filename, add_ref<VertexInput> vi,
    add_ref<Vec3> minPosition, add_ref<Vec3> maxPosition)
{
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> uvs;
	std::vector<uint32_t> indices;
	loadGltfMesh(filename, positions, normals, uvs, indices);
	calcMinMaxPosition(positions, minPosition, maxPosition);
    if (!positions.empty()) {
        std::cout << "First position: " << positions[0] << std::endl;
    }
    vi.binding(0).addAttributes(positions);
    if (!normals.empty()) {
		ASSERTMSG(normals.size() == positions.size(),
			"Normals number (", normals.size(), ") must be equal to positions number (", positions.size(), ")");
        vi.binding(0).addAttributes(normals);
		std::cout << "First normal: " << normals[0] << std::endl;
    }
    if (!uvs.empty()) {
        ASSERTMSG(uvs.size() == positions.size(),
			"UVs number (", uvs.size(), ") must be equal to positions number (", positions.size(), ")");
        vi.binding(0).addAttributes(uvs);
		std::cout << "First UV: " << uvs[0] << std::endl;
    }
    if (!indices.empty()) {
		ASSERTMSG(indices.size() % 3 == 0, "Indices count (", indices.size(), ") must be a multiple of 3");
		const uint32_t indexCount = data_count(indices);
        ZBuffer indexBuffer = createIndexBuffer(vi.device, indexCount, VK_INDEX_TYPE_UINT32);
        bufferWrite(indexBuffer, indices);
        return { indexBuffer, indexCount };
    }
    return {};
}

std::pair<ZShaderModule, ZShaderModule> createShaders(ZDevice device, add_cref<Params> params)
{
    std::string vertFile = "shader.vert";
    std::string fragFile = "shader.frag";

    vtf::ProgramCollection pc(device, params.assets);

    pc.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, vertFile);
    pc.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, fragFile);

    pc.buildAndVerify(false);

    ZShaderModule vert = pc.getShader(VK_SHADER_STAGE_VERTEX_BIT);
    ZShaderModule frag = pc.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

    return { vert, frag };
}

TriLogicInt runTest(add_ref<Canvas> ctx, add_cref<Params> params)
{
    // Print current running device information
    {
        const auto p = deviceGetPhysicalProperties(ctx.device);
        printPhysicalDevice(p, std::cout);
    }

    VertexInput vi(ctx.device);
	Vec3 minPosition, maxPosition;
    ZBuffer indexBuffer; uint32_t indexCount;
    std::tie(indexBuffer, indexCount) = prepareMesh(params.meshFilename, vi, minPosition, maxPosition);
	//std::tie(indexBuffer, indexCount) = prepareTestMesh(vi, minPosition, maxPosition);
	std::cout << "Min position: " << minPosition << ", Max position: " << maxPosition << std::endl;

	ZCommandPool cmdPool = createCommandPool(ctx.device, ctx.graphicsQueue);
	LayoutManager lm(ctx.device, cmdPool);

    struct Uni
    {
		glm::mat4 model;
		glm::mat4 mvp;
        glm::mat4 proj;
    };
	ZBuffer uni = createBuffer<Uni>(ctx.device, 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const uint32_t uniBinding = lm.addBinding(uni, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	ZDescriptorSetLayout dsLayout = lm.createDescriptorSetLayout();
    ZPipelineLayout pLayout = lm.createPipelineLayout({dsLayout});

    const VkFormat				format = ctx.surfaceFormat;
    const VkClearValue			clearColor{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
    const std::vector<RPA>		colors{ RPA(AttachmentDesc::Presentation, format, clearColor,
                                                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) };
    const ZAttachmentPool		attachmentPool(colors);
    const ZSubpassDescription2	subpass({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
    ZRenderPass					renderPass = createRenderPass2(ctx.device, attachmentPool, subpass);

    ZShaderModule vert, frag;
    std::tie(vert, frag) = createShaders(ctx.device, params);

    ZPipeline pipeline = createGraphicsPipeline(pLayout, renderPass, vert, frag,
                                                vi, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT,
        ZCullModeFlags(VK_CULL_MODE_NONE), VK_FRONT_FACE_COUNTER_CLOCKWISE);

	auto updateUniforms = [&](add_cref<Canvas> cs)
    {
        const glm::mat4 model(1.0f);
        // const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f));
        // glm::lookAt(pozycja_kamery, na_co_patrzy, wektor_góry)

        glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 15.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        float aspect = (float)cs.width / (float)cs.height;
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

	// Y sign correction for Vulkan
        // proj[1][1] *= -1;
        const Uni u{ model, view, proj };
        lm.writeBinding(uniBinding, u);
    };

    int drawTrigger = 1;
    ctx.events().setDefault(drawTrigger);

    auto onCommandRecording = [&](add_ref<Canvas> cs, add_cref<Canvas::Swapchain> swapchain,
                                    ZCommandBuffer cmd, ZFramebuffer framebuffer)
    {
        //ZImage swapImage = framebufferGetImage(framebuffer, 0);
        updateUniforms(cs);
        commandBufferBegin(cmd);
		commandBufferBindPipeline(cmd, pipeline);
        commandBufferBindVertexBuffers(cmd, vi);
        commandBufferBindIndexBuffer(cmd, indexBuffer);
        commandBufferSetScissor(cmd, swapchain);
        commandBufferSetViewport(cmd, swapchain);
        auto rpbi = commandBufferBeginRenderPass(cmd, framebuffer);
            vkCmdDrawIndexed(*cmd, indexCount, 1u, 0u, 0u, 0u);
		commandBufferEndRenderPass(rpbi);
		commandBufferEnd(cmd);
    };

    return ctx.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<SUZANNE_GLTF>
{
	static bool record(TestRecord&);
};
bool TestRecorder<SUZANNE_GLTF>::record(TestRecord& record)
{
	record.name = "suzanne_gltf";
	record.call = &prepareTest;
	return true;
}
