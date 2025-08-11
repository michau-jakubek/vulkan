#include "triangleTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfZPipeline.hpp"
#include <type_traits>
#include <thread>

namespace
{
using namespace vtf;

uint32_t userRunOnThreads (const strings& params, std::ostream& log)
{
	bool				status = false;
	strings				sink;
	strings				args(params);
	Option				runOnThreads { "-t", 1 };
	std::vector<Option>	options { runOnThreads };
	uint32_t			threads = 0;
	if (consumeOptions(runOnThreads, options, args, sink) > 0)
	{
		threads = fromText(sink.back(), 2u, status);
		if (!status)
		{
			log << "[ERROR] Unable to parse thread count" << std::endl;
		}
	}
	if (status && threads < 2)
	{
		log << "[WARNING] Automatically set minimum thread count to 2" << std::endl;
		threads = 0;
	}
	return threads;
}

bool userInfinityRepeat (const strings& params)
{
	strings				sink;
	strings				args(params);
	Option				runOnThreads { "-i", 0 };
	std::vector<Option>	options { runOnThreads };
	return (consumeOptions(runOnThreads, options, args, sink) > 0);
}

std::vector<ZQueue> buildQueues (ZDevice device, uint32_t queueCount)
{
	std::vector<ZQueue> queues;
	for (uint32_t i = 0; i < queueCount; ++i)
		queues.emplace_back(deviceGetNextQueue(device, VK_QUEUE_GRAPHICS_BIT, false));
	return queues;
}

bool verifyQueues (add_cref<std::vector<ZQueue>> queues)
{
	for (ZQueue q : queues)
	{
		if (!q.has_handle())
			return false;
	}
	return true;
}

TriLogicInt runTriangeSingleThread (Canvas& canvas, const std::string& assets, bool infinityRepeat);
TriLogicInt runTriangleMultipleThreads (Canvas& canvas, const std::string& assets, uint32_t threadCount);
TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	UNREF(cmdLineParams);
	std::cout << "Parameters"									<< std::endl;
	std::cout << "  [-t <num>]  run on threads, default is 2"	<< std::endl;
	std::cout << "  [-i]        infinity repeat"				<< std::endl;
	std::cout << "Navigation keys"								<< std::endl;
	std::cout << "  Escape: quit this app"						<< std::endl;
    add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, nullptr, gf.apiVer);
	const uint32_t threadCount = userRunOnThreads(cmdLineParams, std::cout);
	return (threadCount >= 2)
			? runTriangleMultipleThreads(cs, record.assets, threadCount)
			: runTriangeSingleThread(cs, record.assets, userInfinityRepeat(cmdLineParams));
}

TriLogicInt runTriangeSingleThread (Canvas& cs, const std::string& assets, bool infinityRepeat)
{
	LayoutManager				pl			(cs.device);
	ProgramCollection			programs	(cs.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags		(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);

	ZShaderModule				vertShaderModule	= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule	= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	VertexInput					vertexInput			(cs.device);
	{
		std::vector<Vec2>		vertices			{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
		std::vector<Vec3>		colors				{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		vertexInput.binding(0).addAttributes(vertices, colors);
	}

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});
	ZPipelineLayout				pipelineLayout		= pl.createPipelineLayout();
	ZPipeline					mainThreadPipeline	= createGraphicsPipeline(pipelineLayout, renderPass,
															vertexInput, vertShaderModule, fragShaderModule,
															VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);

	int drawTrigger = 1;
	cs.events().setDefault(drawTrigger);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain,
									ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		if (infinityRepeat)
		{
			drawTrigger = drawTrigger + 1;
		}

		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, mainThreadPipeline);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &swapchain.scissor);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer, 0);
				vkCmdDraw(*cmdBuffer, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
			commandBufferMakeImagePresentationReady(cmdBuffer, framebufferGetImage(framebuffer));
		commandBufferEnd(cmdBuffer);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

TriLogicInt runTriangleMultipleThreads (Canvas& cs, const std::string& assets, const uint32_t threadCount)
{
	std::vector<ZQueue> queues	= buildQueues(cs.device, threadCount);
	if (!verifyQueues(queues))
	{
		std::cout << "[ERROR} Unable to test on threads due to lack of hardware threads." << std::endl;
		return (1);
	}

	ProgramCollection			programs(cs.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate);

	ZShaderModule				vertShaderModule	= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule	= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	VertexInput					vertexInput			(cs.device);
	{
		std::vector<Vec2>		vertices			{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
		std::vector<Vec3>		colors				{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		vertexInput.binding(0).addAttributes(vertices, colors);
	}

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZPipelineLayout				pipelineLayout		= LayoutManager(cs.device).createPipelineLayout();

	int drawTrigger = 1;
	cs.events().setDefault(drawTrigger);

	const uint32_t	blitImageWidth	= 1024;
	const uint32_t	blitImageHeight	= 1024;
	ZRenderPass		renderPass		= createColorRenderPass(cs.device, {format}, {{clearColor}});
	struct ThreadLocal
	{
		ZPipeline		pipeline;
		ZImage			image;
		ZImageView		view;
		ZFramebuffer	framebuffer;
	};
	auto createThreadLocal = [&]() -> ThreadLocal
	{
		ThreadLocal data;
		data.pipeline		= createGraphicsPipeline(pipelineLayout, renderPass,
													 vertexInput, vertShaderModule, fragShaderModule,
													 makeExtent2D(blitImageWidth, blitImageHeight));
		data.image			= cs.createColorImage2D(format, blitImageWidth, blitImageHeight);
		data.view			= createImageView(data.image);
		data.framebuffer	= createFramebuffer(renderPass, blitImageWidth, blitImageHeight, {data.view});
		return data;

	};
	auto threadsData = createVector(threadCount, createThreadLocal);

	auto onCommandRecordingThenBlit = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmdBuffer, uint32_t threadID)
	{
		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, threadsData[threadID].pipeline);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, threadsData[threadID].framebuffer, 0);
				vkCmdDraw(*cmdBuffer, 3, 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
		commandBufferEnd(cmdBuffer);

		return threadsData[threadID].image;
	};

	return cs.run(onCommandRecordingThenBlit, queues);
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
