#include "banalRenderPassTests.hpp"
#include "vtfContext.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfZRenderPass2.hpp"
#include "vtfZPipeline.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZImage.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfPrettyPrinter.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	Params(add_cref<std::string> assets_)
		: assets(assets_)
	{
	}
};

TriLogicInt runTests(add_ref<VulkanContext> canvas, add_cref<Params> params);
TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME).checkSupported();

		caps.addExtension(VK_KHR_MAINTENANCE2_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
			.checkSupported("synchronization2");
		caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();
	};

	VulkanContext ctx(record.name, gf.layers, {}, {}, onEnablingFeatures, Version(1, 3), gf.debugPrintfEnabled);

	return runTests(ctx, params);
}


TriLogicInt runTests(add_ref<VulkanContext> canvas, add_cref<Params> params)
{
	ProgramCollection			programs(canvas.device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader1.frag");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader2.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);
	ZShaderModule				vertShaderModule = programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule = programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0u);
	ZShaderModule				frag1ShaderModule = programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1u);
	ZShaderModule				frag2ShaderModule = programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 2u);

	VertexInput					vertexInput(canvas.device);
	{
		std::vector<Vec2>		vertices{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
		vertexInput.binding(0).addAttributes(vertices);
	}

	const auto ex = makeExtent2D(8, 10);
	ZCommandPool cmdPool = createCommandPool(canvas.device, canvas.graphicsQueue);
	LayoutManager lm0(canvas.device, cmdPool);
	LayoutManager lm1(canvas.device, cmdPool);
	LayoutManager lm2(canvas.device, cmdPool);

	std::vector<RPA> colors0
	{
		RPA(AttachmentDesc::Color, VK_FORMAT_R32_UINT, makeClearColor(UVec4(1u)), VK_IMAGE_LAYOUT_UNDEFINED),
		RPA(AttachmentDesc::Color, VK_FORMAT_R32_UINT, makeClearColor(UVec4(2u)), VK_IMAGE_LAYOUT_UNDEFINED),
		RPA(AttachmentDesc::Input, 0u),
		RPA(AttachmentDesc::Input, 1u),
	};
	ZAttachmentPool pool0(colors0);
	ZImage img0 = canvas.createColorImage2D(VK_FORMAT_R32_UINT, ex);
	ZImage img1 = canvas.createColorImage2D(VK_FORMAT_R32_UINT, ex);
	ZBuffer buff0 = createBuffer(img0);
	ZBuffer buff1 = createBuffer(img1);
	ZImageView view0 = createImageView(img0);
	ZImageView view1 = createImageView(img1);
	const uint32_t v0_binding = lm1.addBinding(view0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t v1_binding = lm1.addBinding(view1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t b0_binding = lm1.addBinding(buff0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t b1_binding = lm1.addBinding(buff1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	ZSubpassDescription2 desc0(
		{ RPAR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
		  RPAR(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) });
	ZSubpassDescription2 desc1(
		{ RPAR(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		  RPAR(3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) });
	ZSubpassDependency2 dep0rp0(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								VK_ACCESS_NONE, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	ZSubpassDependency2 dep1rp0(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	ZSubpassDependency2 dep2rp0(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
								VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_ACCESS_NONE);
	ZRenderPass rp0 = createRenderPass2(canvas.device, pool0, dep0rp0, desc0, dep1rp0, desc1, dep2rp0);
	struct PC {
		uint32_t width;
	} pc0{ 123 }, pc1{ ex.width }, pc2{ ex.width };
	ZPushRange<PC> range(VK_SHADER_STAGE_FRAGMENT_BIT);
	ZPipelineLayout layout0 = lm0.createPipelineLayout(range);
	ZDescriptorSetLayout dsLayout1 = lm1.createDescriptorSetLayout();
	ZPipelineLayout layout1 = lm1.createPipelineLayout({ dsLayout1 }, range);

	ZPipeline pipeline0 = createGraphicsPipeline(layout0, ex, rp0, vertexInput,
							vertShaderModule, fragShaderModule, gpp::SubpassIndex(0));
	ZPipeline pipeline1 = createGraphicsPipeline(layout1, ex, rp0, vertexInput,
							vertShaderModule, frag1ShaderModule, gpp::SubpassIndex(1));
	ZFramebuffer framebuffer0 = createFramebuffer(rp0, ex, { view0, view1 });



	ZBuffer buff2 = createBuffer(img0);
	ZBuffer buff3 = createBuffer(img1);
	const uint32_t v2_binding = lm2.addBinding(view0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t v3_binding = lm2.addBinding(view1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
										VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t b2_binding = lm2.addBinding(buff2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	const uint32_t b3_binding = lm2.addBinding(buff3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	ZDescriptorSetLayout dsLayout2 = lm2.createDescriptorSetLayout();
	ZPipelineLayout layout2 = lm2.createPipelineLayout({ dsLayout2 }, range);
	{
		std::vector<uint32_t> v(lm2.getBindingElementCount(b2_binding));
		std::fill(v.begin(), v.end(), 18u);
		lm2.writeBinding(b2_binding, v);
		std::fill(v.begin(), v.end(), 19u);
		lm2.writeBinding(b3_binding, v);
	}
	std::vector<RPA> colors2
	{
		RPA(AttachmentDesc::Color, VK_FORMAT_R32_UINT, makeClearColor(UVec4(13u)),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		RPA(AttachmentDesc::Color, VK_FORMAT_R32_UINT, makeClearColor(UVec4(14u)),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		RPA(AttachmentDesc::Input, 0u),
		RPA(AttachmentDesc::Input, 1u),
	};
	ZAttachmentPool pool2(colors2);
	ZSubpassDependency2 dep0rp2(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								VK_ACCESS_NONE, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	ZSubpassDescription2 desc2(
		{ RPAR(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		  RPAR(3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) });
	ZRenderPass rp2 = createRenderPass2(canvas.device, pool2, desc2);
	ZPipeline pipeline2 = createGraphicsPipeline(layout2, ex, rp2, vertexInput,
							vertShaderModule, frag2ShaderModule, gpp::SubpassIndex(0));
	ZFramebuffer framebuffer2 = createFramebuffer(rp2, ex, { view0, view1 });

	ZImageMemoryBarrier bar0(img0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	ZImageMemoryBarrier bar1(img1, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	{
		OneShotCommandBuffer cmd(cmdPool);
		commandBufferBindVertexBuffers(cmd, vertexInput);
		{
			auto rpbi = commandBufferBeginRenderPass(cmd, framebuffer0);
			commandBufferBindPipeline(cmd, pipeline0);
			commandBufferPushConstants(cmd, layout0, pc0);
			vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferNextSubpass(rpbi);
			commandBufferBindPipeline(cmd, pipeline1);
			commandBufferBindVertexBuffers(cmd, vertexInput);
			commandBufferPushConstants(cmd, layout1, pc1);
			vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
		}
		commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, bar0, bar1);
		{
			auto rpbi = commandBufferBeginRenderPass(cmd, framebuffer2);
			commandBufferBindPipeline(cmd, pipeline2);
			commandBufferPushConstants(cmd, layout2, pc2);
			vkCmdDraw(**cmd, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
		}
	}

	PrettyPrinter printer;

	auto writeResult = [&](add_cref<LayoutManager> lm, uint32_t binding, uint32_t cursor, add_cptr<char> title)
	{
		std::vector<uint32_t> data;
		lm.readBinding(binding, data);

		printer.getCursor(cursor) << title << ", binding = " << binding << std::endl;

		for (uint32_t y = 0; y < ex.height; ++y)
		{
			for (uint32_t x = 0u; x < ex.width; ++x)
			{
				printer.getCursor(cursor) << std::setw(3) << data[y * ex.width + x] << std::setw(0) << ' ';
			}
			printer.getCursor(cursor) << std::endl;
		}
		printer.getCursor(cursor) << std::endl;
	};

	writeResult(lm1, v0_binding, 0, "Image0");
	writeResult(lm1, v1_binding, 1, "Image1");
	writeResult(lm1, b0_binding, 2, "Buffer0");
	writeResult(lm1, b1_binding, 3, "Buffer1");
	writeResult(lm2, b2_binding, 4, "Buffer2");
	writeResult(lm2, b3_binding, 5, "Buffer3");

	MULTI_UNREF(v2_binding, v3_binding);

	printer.merge({ 0, 1 }, std::cout);
	printer.merge({ 2, 3 }, std::cout);
	printer.merge({ 4, 5 }, std::cout);

	return {};
}

} // unnamed namespace

template<> struct TestRecorder<BANAL_RENDERPASS>
{
	static bool record(TestRecord&);
};
bool TestRecorder<BANAL_RENDERPASS>::record(TestRecord& record)
{
	record.name = "banal_renderpass";
	record.call = &prepareTests;
	return true;
}
