#include "banalDRLRTests.hpp"
#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCopyUtils.hpp"

namespace
{

using namespace vtf;

static constexpr uint32_t IMAGE_WIDTH = 256;
static constexpr uint32_t IMAGE_HEIGHT = 256;

struct Params
{
	add_cref<std::string> assets;
	Params(add_cref<std::string> assets_)
		: assets(assets_) {}
};

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
std::array<ZShaderModule, 3> buildProgram (ZDevice device, add_cref<Params> params);
void genVertices (VertexInput& vi);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	auto onEnablingFeatures = [](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceVulkan14Features::dynamicRenderingLocalRead)
			.checkSupported("dynamicRenderingLocalRead");
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
			.checkSupported("dynamicRendering");
		caps.addExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387] | MessageID = 0x12537a2c
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[1] Missing extension required by the device
		// extension VK_KHR_dynamic_rendering : VK_KHR_depth_stencil_resolve.
		caps.addExtension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME).checkSupported();

		// Validation Error : [VUID - vkCreateDevice - ppEnabledExtensionNames - 01387] | MessageID = 0x12537a2c
		// vkCreateDevice() : pCreateInfo->ppEnabledExtensionNames[2] Missing extension required by the device
		// extension VK_KHR_depth_stencil_resolve : VK_KHR_create_renderpass2.
		caps.addExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState)
			.checkSupported("extendedDynamicState");
		caps.addExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME).checkSupported();
	};

	Params params(record.assets);

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.width = IMAGE_WIDTH;
	canvasStyle.height = IMAGE_HEIGHT;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	Canvas canvas(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return runTests(canvas, params);
}

void genVertices (VertexInput& vi)
{
	const std::vector<Vec2> pos{ { -1, +1 }, { -1, -1 }, { +1, +1 }, { +1, -1 } };
	vi.binding(0).addAttributes(pos, pos);
}

std::array<ZShaderModule, 3> buildProgram (ZDevice device, add_cref<Params> params)
{
	ProgramCollection progs(device, params.assets);
	progs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	progs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader0.frag");
	progs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader1.frag");
	progs.buildAndVerify(true);
	return
	{
		progs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		progs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		progs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
	};
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	VertexInput	vertexInput(canvas.device);
	genVertices(vertexInput);

	auto [vert, frag0, frag1] = buildProgram(canvas.device, params);

	const Vec4 clearColor(0, 1, 0, 1);
	const std::vector<VkClearValue> clearColors{ makeClearColor(clearColor), makeClearColor(clearColor) };

	ZImage image00 = canvas.createColorImage2D(VK_FORMAT_R32G32B32A32_SFLOAT, IMAGE_WIDTH, IMAGE_HEIGHT);
	ZImage image02 = canvas.createColorImage2D(VK_FORMAT_R32G32B32A32_SFLOAT, IMAGE_WIDTH, IMAGE_HEIGHT);
	ZImage image10 = canvas.createColorImage2D(VK_FORMAT_R32G32B32A32_SFLOAT, IMAGE_WIDTH, IMAGE_HEIGHT);
	ZImage image12 = canvas.createColorImage2D(VK_FORMAT_R32G32B32A32_SFLOAT, IMAGE_WIDTH, IMAGE_HEIGHT);
	if (false)
	{
		std::cout << "image00: " << image00 << std::endl;
		std::cout << "image02: " << image02 << std::endl;
		std::cout << "image10: " << image10 << std::endl;
		std::cout << "image12: " << image12 << std::endl;
	}
	ZImageView view00 = createImageView(image00);
	ZImageView view02 = createImageView(image02);
	ZImageView view10 = createImageView(image10);
	ZImageView view12 = createImageView(image12);

	ZBuffer output = createBuffer(image00);

	LayoutManager				lm0			(canvas.device);
	ZPipelineLayout				layout0		= lm0.createPipelineLayout();
	ZPipeline					pipeline0	= createGraphicsPipeline(layout0, vert, frag0, vertexInput,
													std::make_tuple(view00,			2u, true),
													std::make_tuple(ZImageView(),	1u, true),
													std::make_tuple(view02,			0u, true),
													makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT),
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	LayoutManager				lm1			(canvas.device);
	lm1.addBinding(view00, {}, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, shaderGetStage(frag1));
	lm1.addBinding(view02, {}, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, shaderGetStage(frag1));
	ZDescriptorSetLayout		dsLayout	= lm1.createDescriptorSetLayout();
	ZPipelineLayout				layout1		= lm1.createPipelineLayout({ dsLayout });
	ZPipeline					pipeline1	= createGraphicsPipeline(layout1, vert, frag1, vertexInput,
													std::make_tuple(view10,			2u, true),
													std::make_tuple(ZImageView(),	1u, true),
													std::make_tuple(view12,			0u, true),
													std::make_tuple(view00,			2u, false),
													std::make_tuple(ZImageView(),	1u, false),
													std::make_tuple(view02,			0u, false),
													makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT),
													VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

	const VkFormat				format		= canvas.surfaceFormat;
	ZRenderPass					renderPass	= createColorRenderPass(canvas.device, { format }, { {clearColors[0]} });

	int drawTrigger = 1;
	canvas.events().setDefault(drawTrigger);

	ZImageMemoryBarrier gen00(image00, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	ZImageMemoryBarrier gen02(image02, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	ZImageMemoryBarrier gen10(image10, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	ZImageMemoryBarrier gen12(image12, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	ZImageMemoryBarrier inp0(image00, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);
	ZImageMemoryBarrier inp2(image02, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>,
									ZCommandBuffer cmd, ZFramebuffer framebuffer)
	{
		ZImage displayImage = framebufferGetImage(framebuffer);

		commandBufferBegin(cmd);

		// Below call makes no sense when pipeline is bound.
		// commandBufferSetDefaultDynamicStates(cmd, vertexInput, makeViewport(IMAGE_WIDTH, IMAGE_HEIGHT));

		commandBufferBindVertexBuffers(cmd, vertexInput);

		commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, gen00, gen02, gen10, gen12);

		commandBufferBeginRendering(cmd, IMAGE_WIDTH, IMAGE_HEIGHT,	{ view00, ZImageView(), view02 }, clearColors);
		commandBufferBindPipeline(cmd, pipeline0);
		vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 1u, 0u, 0u);
		commandBufferEndRendering(cmd);

		commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, inp0, inp2);

		commandBufferBeginRendering(cmd, IMAGE_WIDTH, IMAGE_HEIGHT, { view10, ZImageView(), view12 }, clearColors);
		commandBufferBindPipeline(cmd, pipeline1);
		vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 1u, 0u, 0u);
		commandBufferEndRendering(cmd);

		ZImage srcImage = image10;

		imageCopyToBuffer(cmd, srcImage, output,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_ACCESS_NONE, VK_ACCESS_HOST_READ_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		commandBufferBlitImageWithBarriers(cmd, srcImage, displayImage,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
			VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		commandBufferEnd(cmd);
	};

	std::cout << "Press Escape to finish application" << std::endl;
	const int runRes = canvas.run(onCommandRecording, renderPass, std::ref(drawTrigger));

	auto verifyOutput = [](ZBuffer buffer, add_cref<Vec4> color1, add_cref<Vec4> color2) -> uint32_t
	{
		uint32_t mismatch = 0u;
		BufferTexelAccess<Vec4> c(buffer, IMAGE_WIDTH, IMAGE_HEIGHT);
		for (uint32_t y = 0; y < IMAGE_HEIGHT; ++y)
		for (uint32_t x = 0; x < IMAGE_WIDTH; ++x)
		{
			const Vec4 color = c.at(x, y);
			if (x >= (IMAGE_WIDTH / 4) && x < ((IMAGE_WIDTH * 3) / 4)
				&& y >= (IMAGE_HEIGHT / 4) && y < ((IMAGE_HEIGHT * 3) / 4))
			{
				if (color != color1)
					++mismatch;
			}
			else if (color != color2)
			{
				++mismatch;
			}
		}
		return mismatch;
	};

	const uint32_t mismatch1 = verifyOutput(output, Vec4(1, 0, 0, 1), clearColor);

	OneShotCommandBuffer cmd(canvas.device, canvas.graphicsQueue);
	imageCopyToBuffer(cmd, image00, output,
		VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_IMAGE_LAYOUT_GENERAL);
	cmd.endRecordingAndSubmit();

	const uint32_t mismatch0 = verifyOutput(output, Vec4(0, 0, 1, 1), clearColor);

	return (runRes == 0 && mismatch0 == 0 && mismatch1 == 0) ? 0 : 1;
}

} // unnamed namespace

template<> struct TestRecorder<BANAL_DRLR>
{
	static bool record(TestRecord&);
};
bool TestRecorder<BANAL_DRLR>::record(TestRecord& record)
{
	record.name = "banal_drlr";
	record.call = &prepareTests;
	return true;
}
