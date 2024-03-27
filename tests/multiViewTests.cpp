#include "triangleTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfMatrix.hpp"
#include "vtfZPipeline.hpp"
#include "vtfStructUtils.hpp"
#include <bitset>
#include <type_traits>
#include <thread>

namespace
{
using namespace vtf;

std::tuple<bool, uint32_t, uint32_t>
paramGetLayersAndIndexType (const strings& cmdLineParams, std::ostream& log,
							uint32_t defaultLayers, uint32_t maxLayers,
							uint32_t defaultIndexType)
{
	bool				status = false;
	strings				sink;
	strings				args(cmdLineParams);
	Option				optLayers { "-v", 1 };
	Option				optIndexType { "-i", 1 };
	std::vector<Option>	options { optLayers, optIndexType };
	uint32_t			layers = defaultLayers;
	uint32_t			indexType = defaultIndexType;
	if (consumeOptions(optLayers, options, args, sink) > 0)
	{
		layers = fromText(sink.back(), defaultLayers, status);
		if (!status)
		{
			log << "[WARNING] Unable to parse layer count.\n"
				   "          Assuming default value of " << defaultLayers << std::endl;
		}
	}
	if (status && layers > maxLayers)
	{
		log << "[WARNING] The number of layers of " << layers
			<< " exceeds max number of layers of " << maxLayers
			<< "\n          Assuming maximum available layers" << std::endl;
		layers = maxLayers;
	}
	if (status && layers < defaultLayers)
	{
		log << "[WARNING] The number of layers of " << layers << " is too small.\n"
			<< "          Assuming default value of " << defaultLayers << std::endl;
		layers = defaultLayers;
	}
	if (consumeOptions(optIndexType, options, args, sink) > 0)
	{
		indexType = fromText(sink.back(), defaultIndexType, status);
		if (!status)
		{
			log << "[WARNING] Unable to parse index type.\n"
				   "        Assuming default value of " << defaultIndexType << std::endl;
		}
	}
	status = true;
	if (!args.empty())
	{
		status = false;
		log << "[ERROR] Unknown parameter: " << args[0] << std::endl;
	}
	return { status, layers, indexType };
}

std::pair<uint32_t, uint32_t> printUsage (std::ostream& log)
{
	const uint32_t defaultLayers = 2u;
	const uint32_t defaultIndexType = 0u;
	log << "Parameters:\n"
		<< "  [-v <uint:num>]        view count, default is " << defaultLayers << '\n'
		<< "  [-i <uint:index_type>] type of index used by shader\n"
		<< "                         If differs from 0 then gl_DeviceIndex,\n"
		<< "                         otherwise gl_ViewIndex will be used.\n"
		<< "                         Default value is " << defaultIndexType << '\n'
		<< "Navigation keys:\n"
		<< "  Escape: quit this app"
		<< std::endl;
	return { defaultLayers, defaultIndexType };
}

uint32_t printMultiDrawProperties (ZPhysicalDevice device, std::ostream& log)
{
	VkPhysicalDeviceMultiviewProperties	multiviewProps	= makeVkStruct();
	VkPhysicalDeviceProperties2			deviceProps		= makeVkStruct(&multiviewProps);
	vkGetPhysicalDeviceProperties2(*device, &deviceProps);
	log << "VkPhysicalDeviceMultiviewProperties {"	<< std::endl;
	log << "  uint32_t maxMultiviewViewCount     = " << multiviewProps.maxMultiviewViewCount << std::endl;
	log << "  uint32_t maxMultiviewInstanceIndex = " << multiviewProps.maxMultiviewInstanceIndex << std::endl;
	log << "}" <<std::endl;
	return multiviewProps.maxMultiviewViewCount;
}

TriLogicInt runMultiViewSingleThread (Canvas& canvas, const std::string& assets, const uint32_t multiLayerCount, bool useDeviceIndex);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	VkPhysicalDeviceMultiviewFeatures multiviewFeatures = makeVkStruct();

	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		VkPhysicalDeviceFeatures2 resultFeatures = makeVkStruct();

		if (containsString(VK_EXT_MULTI_DRAW_EXTENSION_NAME, extensions))
		{
			resultFeatures.pNext = &multiviewFeatures;
			vkGetPhysicalDeviceFeatures2(*physicalDevice, &resultFeatures);
		}

		return resultFeatures;
	};
	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);
	if (!multiviewFeatures.multiview)
	{
		std::cout << "[ERROR] VK_EXT_multi_draw not supported by device" << std::endl;
		return 1;
	}

	std::ostream& log = std::cout;
	const auto defaults = printUsage(log);
	const uint32_t maxLayers = printMultiDrawProperties(cs.physicalDevice, log);
	const auto params = paramGetLayersAndIndexType(cmdLineParams, log, defaults.first, maxLayers, defaults.second);
	if (!std::get<0>(params)) return 1;
	return runMultiViewSingleThread(cs, record.assets, std::get<1>(params), (std::get<2>(params) != 0u));
}

void onResize (Canvas& cs, void* userData, int width, int height)
{
	UNREF(cs);
	UNREF(width);
	UNREF(height);
	if (userData)
	{
		*((int*)userData) += 1;
	}
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

uint32_t makeViewMask (uint32_t value)
{
	std::bitset<32> bits;
	for (uint32_t b = 0; b < value; ++b)
		bits.set(b);
	return static_cast<uint32_t>(bits.to_ulong());
}

std::tuple<ZShaderModule, ZShaderModule> buildProgram (ZDevice device, const std::string& assets, bool useDeviceIndex)
{
	ProgramCollection			programs(device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, useDeviceIndex ? "gl_DeviceIndex.vert" : "gl_ViewIndex.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate);

	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT)
	};
}

void populateVertexAttributes (add_ref<VertexInput> vertexInput)
{
	std::vector<Vec2>		vertices	{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
	std::vector<Vec3>		colors		{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	vertexInput.binding(0).addAttributes(vertices, colors);
}

TriLogicInt runMultiViewSingleThread (Canvas& cs, const std::string& assets, const uint32_t multiLayerCount, bool useDeviceIndex)
{
	LayoutManager				pl	(cs.device);

	ZShaderModule				vertShaderModule, fragShaderModule;
	std::tie(vertShaderModule, fragShaderModule) = buildProgram(cs.device, assets, useDeviceIndex);

	VertexInput					vertexInput			(cs.device);
	populateVertexAttributes(vertexInput);

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };

	const uint32_t				multiImageWidth		= 256;
	const uint32_t				multiImageHeight	= 256;
	ZImage						multiImage			= createImage(cs.device, format, VK_IMAGE_TYPE_2D,
																  multiImageWidth, multiImageHeight,
																  ZImageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_SAMPLED_BIT),
																  VK_SAMPLE_COUNT_1_BIT, /*mipLevels*/1u, /*layers*/multiLayerCount);
	ZImageView					multiImageView		= createImageView(multiImage, 0u, 1u, 0u, multiLayerCount, format, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	ZImageMemoryBarrier			multiImagePreBlit	(multiImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
													 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
													 imageMakeSubresourceRange(multiImage, 0u, 1u, 0u, multiLayerCount));
	ZSubpassDependency			multiSubpass		(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
													 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
													 ZSubpassDependency::Between,
													 VK_DEPENDENCY_BY_REGION_BIT, makeViewMask(multiLayerCount));
	ZRenderPass					multiRenderPass		= createMultiViewRenderPass(cs.device, {format}, {{clearColor}}, {multiSubpass},
																				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	multiSubpass = ZSubpassDependency::makeBegin(makeViewMask(multiLayerCount));
	ZFramebuffer				multiFramebuffer	= createFramebuffer(multiRenderPass, multiImageWidth, multiImageHeight, { multiImageView });
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});
	struct						Binding0Type		{ Mat4 rotateMatrix[32]; } binding0Data;
	const uint32_t				binding0Number		= pl.addBinding<Binding0Type>();
	struct						Binding1Type		{ Mat4 translateMatrix, scaleMatrix; } binding1Data;
	const uint32_t				binding1Number		= pl.addBinding<Binding1Type>();
	ZDescriptorSetLayout		dsLayout			= pl.createDescriptorSetLayout();
	ZPipelineLayout				pipelineLayout		= pl.createPipelineLayout(dsLayout);
	ZPipelineCreateFlags		pipelineCreateFlags	= useDeviceIndex
														? ZPipelineCreateFlags(VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT)
														: ZPipelineCreateFlags();
	ZPipeline					pipelineFirstView	= createGraphicsPipeline(pipelineLayout, multiRenderPass,
															vertexInput, vertShaderModule, fragShaderModule,
															makeExtent2D(multiImageWidth, multiImageHeight),
															pipelineCreateFlags);

	uint32_t d = 0;
	auto updateRotationMatrices = [&]()
	{
		for (uint32_t l = 0; l < multiLayerCount; ++l)
		{
			binding0Data.rotateMatrix[l] = Mat4::rotate(
				Vec4(0, 0, ((2.0f * float(l + d)) / float(multiLayerCount)) ));
			d = (d + 1) % 20;
		}
		pl.writeBinding(binding0Number, binding0Data);
	};

	updateRotationMatrices();
	binding1Data.scaleMatrix = Mat4::scale(Vec4(0.125, 0.125, 0.125));
	binding1Data.translateMatrix = Mat4::translate(Vec4(-0.5, -0.5));
	pl.writeBinding(binding1Number, binding1Data);

	int drawTrigger = 1;
	cs.events().cbKey.set(onKey, nullptr);
	cs.events().cbWindowSize.set(onResize, &drawTrigger);

	uint32_t layerIndex = 0u;

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		ZImage				renderImage			= framebufferGetImage(framebuffer);
		ZImageMemoryBarrier	renderImagePreBlit	(renderImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		commandBufferBegin(cmdBuffer);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, multiFramebuffer, 0);
				commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
				commandBufferBindPipeline(cmdBuffer, pipelineFirstView);
				vkCmdDraw(*cmdBuffer, vertexInput.getAttributeCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
			commandBufferPipelineBarriers(cmdBuffer,
										  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
										  VK_PIPELINE_STAGE_TRANSFER_BIT,
										  multiImagePreBlit, renderImagePreBlit);
			commandBufferBlitImage(cmdBuffer, multiImage, renderImage,
								   imageMakeSubresourceLayers(multiImage, 0u, layerIndex, 1u),
								   imageMakeSubresourceLayers(renderImage));
			commandBufferMakeImagePresentationReady(cmdBuffer, renderImage);
		commandBufferEnd(cmdBuffer);

		updateRotationMatrices();
		layerIndex = (layerIndex + 1) % multiLayerCount;
		drawTrigger = 1;
	};

	return cs.run(onCommandRecording, renderPass, std::ref(drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<MULTIVIEW>
{
	static bool record (TestRecord&);
};
bool TestRecorder<MULTIVIEW>::record (TestRecord& record)
{
	record.name = "multiview";
	record.call = &prepareTests;
	return true;
}
