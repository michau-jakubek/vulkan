#include "taskMeshTriangleTests.hpp"
#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfStructUtils.hpp"

namespace
{

using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	Params(add_cref<std::string> assets_)
		: assets(assets_) {}
};

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_ref<CommandLine> cmdLine);
std::tuple<ZShaderModule, ZShaderModule, ZShaderModule> buildProgram (ZDevice device, add_cref<Params> params);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	UNREF(cmdLine);
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	auto onEnablingFeatures = [](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceMeshShaderFeaturesEXT::taskShader).checkSupported("taskShader");
		caps.addUpdateFeatureIf(&VkPhysicalDeviceMeshShaderFeaturesEXT::meshShader).checkSupported("meshShader");
		caps.addExtension(VK_EXT_MESH_SHADER_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_SPIRV_1_4_EXTENSION_NAME).checkSupported();
		caps.addExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME).checkSupported();
	};

	Params params(record.assets);
	const CanvasStyle canvasStyle = Canvas::DefaultStyle;
	Canvas canvas(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return runTests(canvas, params);
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule> buildProgram (ZDevice device, add_cref<Params> params)
{
	ProgramCollection		prog(device, params.assets);
	prog.addFromFile(VK_SHADER_STAGE_TASK_BIT_EXT, "shader.task");
	prog.addFromFile(VK_SHADER_STAGE_MESH_BIT_EXT, "shader.mesh");
	prog.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	prog.buildAndVerify(Version(1,3), Version(1,4), true);
	ZShaderModule			task = prog.getShader(VK_SHADER_STAGE_TASK_BIT_EXT);
	ZShaderModule			mesh = prog.getShader(VK_SHADER_STAGE_MESH_BIT_EXT);
	ZShaderModule			frag = prog.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);
	return { task, mesh, frag };
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface>	di = canvas.device.getInterface();

	ZShaderModule	taskShader, meshShader, fragShader;
	std::tie(taskShader, meshShader, fragShader) = buildProgram(canvas.device, params);

	LayoutManager				lm			(canvas.device);
	const VkFormat				format		= canvas.surfaceFormat;
	const VkClearValue			clearColor	{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZRenderPass					renderPass	= createSinglePresentationRenderPass(canvas.device, format, clearColor);
	ZPipelineLayout				layout		= lm.createPipelineLayout();
	ZPipeline					pipeline	= createGraphicsPipeline(layout, renderPass,
														taskShader, meshShader, fragShader,
														VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);

	int drawTrigger = 1;
	canvas.events().setDefault(drawTrigger);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain,
									ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, pipeline);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &swapchain.scissor);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);
				di.vkCmdDrawMeshTasksEXT(*cmdBuffer, 1u, 1u, 1u);
			commandBufferEndRenderPass(rpbi);
			commandBufferMakeImagePresentationReady(cmdBuffer, framebufferGetImage(framebuffer));
		commandBufferEnd(cmdBuffer);
	};

	return canvas.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<TASK_MESH_TRIANGLE>
{
	static bool record(TestRecord&);
};
bool TestRecorder<TASK_MESH_TRIANGLE>::record (TestRecord& record)
{
	record.name = "task_mesh_triangle";
	record.call = &prepareTests;
	return true;
}
