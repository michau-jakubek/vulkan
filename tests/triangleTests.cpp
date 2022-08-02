#include "triangleTests.hpp"
#include "vtfCanvas.hpp"
//#include "vkPipelineElements.hpp" // do wywalenia
#include "vtfPipelineLayout.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"

namespace
{
using namespace vtf;

int performTests (Canvas& canvas, const std::string& assets);

int prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(cmdLineParams);
	std::cout << "Press Escape to exit" << std::endl;
	Canvas	cs(record.name, record.layers);
	return performTests(cs, record.assets);
}

void onResize (Canvas& cs, void* userData, int width, int height)
{
	UNREF(width);
	UNREF(height);
	*((int*)userData) += cs.backBufferCount;
}

void onKey (Canvas& cs, void* userData, const int key, int scancode, int action, int mods)
{
	UNREF(userData);
	UNREF(scancode);
	UNREF(mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
}

int performTests (Canvas& cs, const std::string& assets)
{
	PipelineLayout				pl(cs);
	ProgramCollection			programs(cs, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	programs.buildAndVerify();

	ZShaderModule				vertShaderModule	= *programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule	= *programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	std::vector<Vec2>			vertices			{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
	std::vector<Vec3>			colors				{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	cs.vertexInput.binding(0).addAttributes(vertices, colors);

	const VkClearValue			clearColor			{ 0.5f, 0.5f, 0.5f, 0.5f };
	ZRenderPass					renderPass			= cs.createRenderPass({cs.format.format}, clearColor);
	ZPipelineLayout				pipelineLayout		= pl.createPipelineLayout();
	ZPipeline					pipeline			= cs.createGraphicsPipeline(pipelineLayout, renderPass, {/*dynamic viewport & scissor*/},
																				vertShaderModule, fragShaderModule);

	int drawTrigger = cs.backBufferCount;
	cs.events().cbKey.set(onKey, nullptr);
	cs.events().cbWindowSize.set(onResize, &drawTrigger);

	auto onCommandRecording = [&](Canvas&, ZCommandBuffer cmdBuffer, uint32_t swapImageIndex)
	{
		const VkRenderPassBeginInfo rpbi = cs.makeRenderPassBeginInfo(renderPass, swapImageIndex);

		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, pipeline);
			commandBufferBindVertexBuffers(cmdBuffer, cs.vertexInput);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &cs.swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &cs.swapchain.scissor);
			vkCmdBeginRenderPass(*cmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdDraw(*cmdBuffer, 3, 1, 0, 0);
			vkCmdEndRenderPass(*cmdBuffer);
			cs.transitionImageForPresent(cmdBuffer, swapImageIndex);
		commandBufferEnd(cmdBuffer);

		return true;
	};

	return cs.run(renderPass, onCommandRecording, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<TRIANGLE>
{
	static bool record (TestRecord&);
};
bool TestRecorder<TRIANGLE>::record (TestRecord& record)
{
	record.name = "triangle";
	record.call = &prepareTests;
	return true;
}
