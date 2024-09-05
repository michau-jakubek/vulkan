#include "lineWidthTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfBacktrace.hpp"
#include "vtfFilesystem.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZPipeline.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfProgramCollection.hpp"

namespace
{
using namespace vtf;

void printUsage (add_ref<std::ostream> log)
{
	log << "Description:\n"
		<< "  The test draws two lines: horizotal and vertical,\n"
		<< "  and optionally verifies they have been drawn properly.\n"
		<< "Requirements:\n"
		<< "  VkPhysicalDeviceFeatures::wideLines\n"
		<< "Excercise:\n"
		<< "  The test shows how to use:\n"
		<< "  * two different graphics pipelines\n"
		<< "    - with different subpasses\n"
		<< "    - with arbitrary vertex input sets\n"
		<< "  * drawing with more than one subpass\n"
		<< "  * uploading a buffer to an image\n"
		<< "  * downloading an image to a buffer\n"
		<< "  * reading pixels from a buffer\n"
		<< "  * blitting images\n"
		<< "  * parsing command line parameters\n"
		<< "  * handling logical device creation\n"
		<< "  * accessing variables in draw callback\n"
		<< "  * using window events\n"
		<< "Parameters:\n"
		<< "  --h         print help\n"
		<< "  --help      print help\n"
		<< "  [-vert      <veritical_line_width>], default 15\n"
		<< "  [-horz      <horizontal_line_width>], default 5\n"
		<< "  [-s]        submit within loop and test, default false\n"
		<< "Navigation keys:\n"
		<< "  R:          redraw window content\n"
		<< "  Escape:     quit this app"
		<< std::endl;
}

struct TestParams
{
	float	verticalWidth;
	float	horizontalWidth;
	bool	submitWithinLoop;
};

TestParams userReadParams (const strings& params, add_ref<std::ostream> log, add_ref<int> ok)
{
	TestParams			p{};
	strings				sink;
	strings				args(params);
	Option				optVerticalWidth	{ "-vert", 1 };
	Option				optHorizontalWidth	{ "-horz", 1 };
	Option				optHelpShort		{ "-h", 0 };
	Option				optHelpLong			{ "--help", 0 };
	Option				optSubmitWithinLoop	{ "-s", 0 };
	std::vector<Option>	options { optVerticalWidth, optHorizontalWidth, optSubmitWithinLoop,
									optHelpShort, optHelpLong };

	if (consumeOptions(optHelpShort, options, args, sink) > 0
		|| consumeOptions(optHelpLong, options, args, sink) > 0)
	{
		ok = 2 - 3;
		return p;
	}

	float verticalWidth = 15;
	if (consumeOptions(optVerticalWidth, options, args, sink) > 0)
	{
		bool status = false;
		verticalWidth = fromText(sink.back(), verticalWidth, status);
		if (!status) {
			log << "[WARNING] Unable to parse vertical line width, apply default " << verticalWidth << std::endl;
		}
	}

	float horizontalWidth = 5;
	if (consumeOptions(optHorizontalWidth, options, args, sink) > 0)
	{
		bool status = false;
		horizontalWidth = fromText(sink.back(), horizontalWidth, status);
		if (!status) {
			log << "[WARNING] Unable to parse horizontal line width, apply default " << horizontalWidth << std::endl;
		}
	}

	bool submitWithinLoop = (consumeOptions(optSubmitWithinLoop, options, args, sink) > 0);

	p.verticalWidth		= verticalWidth;
	p.horizontalWidth	= horizontalWidth;
	p.submitWithinLoop	= submitWithinLoop;

	if (ok = args.empty() ? 1 : 0; ok == 0)
	{
		log << "[ERROR] Unrecognized parameter " << std::quoted(args.at(0)) << std::endl;
	}

	return p;
}

TriLogicInt runTestSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	int ok = 0;
	const TestParams params = userReadParams(cmdLineParams, std::cout, ok);
	if (ok == 0)
	{
		return 1;
	}
	else if (ok < 0)
	{
		printUsage(std::cout);
		return {};
	}

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	VkPhysicalDeviceProperties	properties{};
	VkPhysicalDeviceFeatures	availableFeatures{};
	auto onEnablingFeatures = [&](ZPhysicalDevice dev, add_ref<strings>)
	{
		vkGetPhysicalDeviceProperties(*dev, &properties);
		vkGetPhysicalDeviceFeatures(*dev, &availableFeatures);
		const VkPhysicalDeviceFeatures2	enabledFeatures
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			nullptr,
			availableFeatures
		};
		return enabledFeatures;
	};

	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer);

	if (availableFeatures.wideLines != VK_TRUE)
	{
		std::cout << "VkPhysicalDeviceFeatures::wideLines not supported" << std::endl;
		return {};
	}

	if (!(params.verticalWidth >= properties.limits.lineWidthRange[0])
		&& (params.verticalWidth <= properties.limits.lineWidthRange[1])
		&& (params.horizontalWidth >= properties.limits.lineWidthRange[0])
		&& (params.horizontalWidth <= properties.limits.lineWidthRange[1]))
	{
		std::cout << "[ERROR] Declared vertical line width " << params.verticalWidth
			<< " or horizontal line width " << params.horizontalWidth << std::endl
			<< "        don't match VkPhysicalDeviceLinimits::lineWidthRange"
			<< Vec2(properties.limits.lineWidthRange) << std::endl
			<< "        Try to change -vert or/and -horz params" << std::endl;
		return {};
	}

	return runTestSingleThread(cs, record.assets, params);
}

void onKey (add_ref<Canvas> cs, void* userData, const int key, int scancode, int action, int mods)
{
	UNREF(userData);
	UNREF(scancode);
	UNREF(mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		if (userData) {
			*static_cast<add_ptr<int>>(userData) += 1;
		}
	}
}

void onResize (Canvas& cs, void* userData, int width, int height)
{
	UNREF(cs);
	UNREF(width);
	UNREF(height);
	if (userData) {
		*static_cast<add_ptr<int>>(userData) += 1;
	}
}

int verifyImage (ZCommandPool commandPool, ZImage image, add_cref<TestParams> params)
{
	ZBuffer buffer = createBuffer(image, ZBufferUsageFlags(), ZMemoryPropertyHostFlags);
	imageCopyToBuffer(commandPool, image, buffer);

	const uint32_t imageWidth = imageGetExtent(image).width;
	const uint32_t imageHeight = imageGetExtent(image).height;

	const BufferTexelAccess<Vec4> pba(buffer, imageWidth, imageHeight);

	uint32_t verticalWidth = 0u;
	for (uint32_t x = 0; x < imageWidth; ++x)
	{
		if (pba.at(x, 0u) != Vec4()) verticalWidth += 1;
	}

	uint32_t horizontalWidth = 0u;
	for (uint32_t y = 0; y < imageHeight; ++y)
	{
		if (pba.at(0u, y) != Vec4()) horizontalWidth += 1;
	}

	std::cout << "Vertical: " << verticalWidth << ", Horizontal: " << horizontalWidth << std::endl;

	return (verticalWidth == uint32_t(params.verticalWidth)
		&& horizontalWidth == uint32_t(params.horizontalWidth))
			? 0
			: 1;
}

TriLogicInt runTestSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params)
{
	ProgramCollection			programs(cs.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	add_cref<GlobalAppFlags>	flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);

	ZShaderModule				vertShaderModule	= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule				fragShaderModule	= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	VertexInput					vertexInput			(cs.device);
	{
		std::vector<Vec2>	vertVertices	{ { 0.5, -1 },	{ 0.5, +1 } };
		std::vector<Vec3>	vertColors		{ { 0,0,1 },	{ 1,1,1 } };
		std::vector<Vec2>	horzVertices	{ { -1, 0.5 },	{ +1, 0.5 } };
		std::vector<Vec3>	horzColors		{ { 1,0,0 },	{ 0,1,0 } };
		vertexInput.binding(0).addAttributes(vertVertices, vertColors);
		vertexInput.binding(1).addAttributes(horzVertices, horzColors);
	}

	ZRenderPass		dstRenderPass	= createColorRenderPass(cs.device, {cs.surfaceFormat}, {});
	VkFormat		srcFormat		= VK_FORMAT_R32G32B32A32_SFLOAT;
	uint32_t		srcWidth		= 100;
	uint32_t		srcHeight		= 100;
	ZImage			srcImage		= cs.createColorImage2D(srcFormat, srcWidth, srcHeight);
	ZImageView		srcView			= createImageView(srcImage);
	ZSubpassDependency	dependency	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
									 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									 ZSubpassDependency::Between);
	ZRenderPass		srcRenderPass	= createColorRenderPass(cs.device, {srcFormat}, {},
											VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
											{ dependency });
	ZFramebuffer	srcFramebuffer	= createFramebuffer(srcRenderPass, srcWidth, srcHeight, {srcView});
	ZPipelineLayout	pipelineLayout	= LayoutManager(cs.device).createPipelineLayout();
	ZPipeline		vertPipeline	= createGraphicsPipeline(pipelineLayout, srcRenderPass,
											vertexInput.binding(0), vertShaderModule, fragShaderModule,
											VK_PRIMITIVE_TOPOLOGY_LINE_LIST, gpp::SubpassIndex(0),
											gpp::LineWidth(float(params.verticalWidth)), makeExtent2D(srcWidth, srcHeight));
	ZPipeline		horzPipeline	= createGraphicsPipeline(pipelineLayout, srcRenderPass,
											vertexInput.binding(1),vertShaderModule,fragShaderModule,
											VK_PRIMITIVE_TOPOLOGY_LINE_LIST, gpp::SubpassIndex(1),
											gpp::LineWidth(1.0f), makeExtent2D(srcWidth, srcHeight),
											VK_DYNAMIC_STATE_LINE_WIDTH);

	int drawTrigger = 1;
	cs.events().cbKey.set(onKey, &drawTrigger);
	cs.events().cbWindowSize.set(onResize, &drawTrigger);

	int drawResult = 0;

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmdBuffer, ZFramebuffer dstFramebuffer)
	{
		ZImage				dstImage	= framebufferGetImage(dstFramebuffer);
		ZImageMemoryBarrier	srcBlit		= makeImageMemoryBarrier(srcImage,
												VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
												VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		ZImageMemoryBarrier	dstBlit		= makeImageMemoryBarrier(dstImage,
												VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
												VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferBegin(cmdBuffer);

			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, srcFramebuffer, 0);

				commandBufferBindPipeline(cmdBuffer, vertPipeline);
				vkCmdDraw(*cmdBuffer, vertexInput.getVertexCount(0), 1, 0, 0);

				commandBufferNextSubpass(rpbi);

				vkCmdSetLineWidth(*cmdBuffer, float(params.horizontalWidth));
				commandBufferBindPipeline(cmdBuffer, horzPipeline);
				vkCmdDraw(*cmdBuffer, vertexInput.getVertexCount(1), 1, 0, 0);

			commandBufferEndRenderPass(rpbi);

			if (params.submitWithinLoop)
			{
				commandBufferEnd(cmdBuffer);
				commandBufferSubmitAndWait(cmdBuffer);
				drawResult = verifyImage(commandBufferGetCommandPool(cmdBuffer), srcImage, params);
				commandBufferBegin(cmdBuffer);
			}

			commandBufferPipelineBarriers(cmdBuffer,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, srcBlit, dstBlit);
			commandBufferBlitImage(cmdBuffer, srcImage, dstImage);
			commandBufferMakeImagePresentationReady(cmdBuffer, dstImage);

		commandBufferEnd(cmdBuffer);
	};

	const int runResult = cs.run(onCommandRecording, dstRenderPass, std::ref(drawTrigger));
	return params.submitWithinLoop ? drawResult : runResult;
}

} // unnamed namespace

template<> struct TestRecorder<LINE_WIDTH>
{
	static bool record (TestRecord&);
};
bool TestRecorder<LINE_WIDTH>::record (TestRecord& record)
{
	record.name = "line_width";
	record.call = &prepareTests;
	return true;
}

