#include "shaderObjectTriangleTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfShaderObjectCollection.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVector.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZSpecializationInfo.hpp"
#include "vtfZBarriers.hpp"

#include <type_traits>
#include <thread>

namespace
{
using namespace vtf;

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

struct TestParams
{
	uint32_t	threadCount;
	bool		infinity;
	bool		dontIgnoreSoExtension;
	bool		dontIgnoreVulkanParam;
	bool		dontIgnoreApiParam;
	bool		dontIgnoreSpirvParam;
	bool		disableSoLayer;
	bool		buildAlways;
	bool		useLinkedShaders;

	VkBool32	extendedDynamicState;
	VkBool32	dynamicRendering;
	VkBool32	shaderObject;

	Version		usedApiVersion;
	Version		usedVkVersion;
	Version		usedSpvVersion;

	void updateEffectiveVersions ()
	{
		if (false == dontIgnoreVulkanParam)	usedVkVersion = Version(1, 3);
		if (false == dontIgnoreApiParam)	usedApiVersion = Version(1, 3);
		if (false == dontIgnoreSpirvParam)	usedSpvVersion = Version(1, 3);
	}

	TestParams (add_cref<GlobalAppFlags> flags)
		: threadCount		(1u)
		, infinity			(false)
		, dontIgnoreSoExtension	(false)
		, dontIgnoreVulkanParam	(false)
		, dontIgnoreApiParam	(false)
		, dontIgnoreSpirvParam	(false)
		, disableSoLayer		(false)
		, buildAlways		(false)
		, useLinkedShaders	(false)
		, extendedDynamicState	(VK_FALSE)
		, dynamicRendering		(VK_FALSE)
		, shaderObject			(VK_FALSE)
		, usedApiVersion	(flags.apiVer)
		, usedVkVersion		(flags.vulkanVer)
		, usedSpvVersion	(flags.spirvVer)
	{
	}

	OptionParser<TestParams>	getParser	();
	void						print		(add_ref<std::ostream> str) const;
};
constexpr Option optionThreadCount			{ "-threads", 1 };
constexpr Option optionInfinity				{ "--infinity", 0 };
constexpr Option optionDontIgnoreSoExtension	{ "--dont-ignore-so-extension", 0 };
constexpr Option optionDontIgnoreGlobalApiVer	{ "--dont-ignore-api-param", 0 };
constexpr Option optionDontIgnoreGlobalVkVer	{ "--dont-ignore-vulkan-param", 0 };
constexpr Option optionDontIgnoreGlobalSpvVer	{ "--dont-ignore-spirv-param", 0 };
constexpr Option optionDisableSoLayer		{ "--disable-so-layer", 0 };
constexpr Option optionBuildAlways			{ "--build-always", 0 };
constexpr Option optionUseLinkedShaders		{ "--linked-shaders", 0 };
OptionParser<TestParams> TestParams::getParser ()
{
	OptionFlags					flags	(OptionFlag::PrintDefault);
	OptionParser<TestParams>	parser	(*this);
	parser.addOption(&TestParams::threadCount, optionThreadCount,
		"Run the test on thread(s)", { threadCount }, flags);
	parser.addOption(&TestParams::infinity, optionInfinity,
		"Generate frames in the infinity loop", { infinity }, flags);
	parser.addOption(&TestParams::dontIgnoreApiParam, optionDontIgnoreGlobalApiVer,
		"Don't ignore -api param, if ignored then 1.3 version will be used", { dontIgnoreApiParam }, flags);
	parser.addOption(&TestParams::dontIgnoreVulkanParam, optionDontIgnoreGlobalVkVer,
		"Don't ignore -vulkan param, if ignored then 1.3 version will be used", { dontIgnoreVulkanParam }, flags);
	parser.addOption(&TestParams::dontIgnoreSpirvParam, optionDontIgnoreGlobalSpvVer,
		"Don't ignore -spirv param, if ignored then 1.3 version will be used", { dontIgnoreSpirvParam }, flags);
	parser.addOption(&TestParams::dontIgnoreSoExtension, optionDontIgnoreSoExtension,
		"Don't ignore " VK_EXT_SHADER_OBJECT_EXTENSION_NAME " extension availability", { dontIgnoreSoExtension }, flags);
	parser.addOption(&TestParams::disableSoLayer, optionDisableSoLayer,
		"Disable " VK_LAYER_KHRONOS_SHADER_OBJECT_NAME " layer", { disableSoLayer }, flags);
	parser.addOption(&TestParams::buildAlways, optionBuildAlways,
		"Force to build the shaders always", { buildAlways }, flags);
	parser.addOption(&TestParams::useLinkedShaders, optionUseLinkedShaders,
		"Use linked shaders instead of separate ones", { useLinkedShaders }, flags);

	return parser;
}

TriLogicInt runTriangeSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params);
TriLogicInt runTriangleMultipleThreads (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	add_cref<GlobalAppFlags>	gf		= getGlobalAppFlags();
	TestParams					params	(gf);
	OptionParser<TestParams>	parser	= params.getParser();
	parser.parse(cmdLineParams);
	OptionParserState			state	= parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messagesText() << std::endl;
		if (state.hasErrors) return {};
	}

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
		params.shaderObject = caps.addUpdateFeatureIf(&VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject);
		if (false == params.dontIgnoreSoExtension)
		{
			VkPhysicalDeviceShaderObjectFeaturesEXT sof{};
			sof.shaderObject = VK_TRUE;
			caps.addUpdateFeature(sof);
		}
		else if (false == params.shaderObject)
		{
			ASSERTMSG(params.shaderObject, VK_EXT_SHADER_OBJECT_EXTENSION_NAME, " not supported by device");
		}

		{
			const auto f = &VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering;
			const auto u = caps.addUpdateFeatureIf(f);
			params.dynamicRendering = u;
			caps.addExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		}

		{
			const auto f = &VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState;
			const auto u = caps.addUpdateFeatureIf(f);
			params.extendedDynamicState = u;
			caps.addExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
		}
	};

	strings layers = gf.layers;
	if (false == params.disableSoLayer)
	{
		layers.push_back(VK_LAYER_KHRONOS_SHADER_OBJECT_NAME);
	}

	params.updateEffectiveVersions();

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, layers, strings(), strings(), canvasStyle, onEnablingFeatures, params.usedApiVersion);

	if (false == cs.device.getInterface().isShaderObjectEnabled())
	{
		std::cout << "ERROR: Unable to initialize Vulkan calls for shaderObject" << std::endl;
		return 1;
	}

	return (params.threadCount >= 2)
			? runTriangleMultipleThreads(cs, record.assets, params)
			: runTriangeSingleThread(cs, record.assets, params);
}

void TestParams::print (add_ref<std::ostream> str) const
{
	{
		OptionParser<TestParams> parser = remove_const(*this).getParser();
		const auto options = parser.getOptions();
		for (const auto& option : options)
		{
			str << option->getName() << ": " << option->defaultWriter() << std::endl;
		}
	}

	add_cref<TestParams> params(*this);

	str << VK_EXT_SHADER_OBJECT_EXTENSION_NAME
		<< " enabled: " << boolean(params.shaderObject != VK_FALSE, true) << std::endl;
	str << VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
		<< " enabled: " << boolean(params.dynamicRendering != VK_FALSE, true) << std::endl;
	str << VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME
		<< " enabled: " << boolean(params.extendedDynamicState != VK_FALSE, true) << std::endl;

	str << "Used Api version: "		<< params.usedApiVersion << std::endl;
	str << "Used Vulkan version: "	<< params.usedVkVersion << std::endl;
	str << "Used SPIR-V version: "	<< params.usedSpvVersion << std::endl;
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

TriLogicInt runTriangeSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params)
{
	params.print(std::cout);

	add_cref<ZDeviceInterface>	di				= cs.device.getInterface();
	LayoutManager				lm				(cs.device);
	const GlobalAppFlags		flags			(getGlobalAppFlags());
	const bool					genAssembly		(false);

	ShaderObjectCollection		coll			(cs.device, assets);
	using Link = ShaderObjectCollection::ShaderLink;
	Link						singleVertex	= coll.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	Link						singleFragment	= coll.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	Link						linkVertex		= coll.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	Link						linkFragment	= coll.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag", linkVertex);

	lm.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256u);
	ZDescriptorSetLayout		dsLayout		= lm.createDescriptorSetLayout();
	ZBuffer						testBuffer		= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(0)).buffer;

	//ZPushConstants				pushConstants	{ZPushRange<Vec4>()};

	coll.updateShader			(singleVertex, params.buildAlways, flags.spirvValidate, genAssembly);
	coll.updateShader			(singleFragment, params.buildAlways, flags.spirvValidate, genAssembly);
	coll.updateShader			(singleVertex, ZSpecEntry<int>(13), ZSpecEntry<uint32_t>(17), ZSpecEntry<Vec4>(Vec4()));
	coll.updateShader			(singleFragment, ZSpecEntry<uint8_t>(13), ZSpecEntry<double>(17));
	coll.updateShaders			({ singleVertex, singleFragment } ); // push
	coll.updateShaders			({ singleVertex, singleFragment }, { dsLayout });

	coll.updateShader			(linkVertex, params.buildAlways, flags.spirvValidate, genAssembly);
	coll.updateShader			(linkFragment, params.buildAlways, flags.spirvValidate, genAssembly);
	coll.updateShader			(linkFragment, ZSpecEntry<int>(13), ZSpecEntry<uint32_t>(17), ZSpecEntry<Vec4>(Vec4()));
	coll.updateShader			(linkVertex, ZSpecEntry<uint8_t>(13), ZSpecEntry<double>(17));
	coll.updateShaders			({ linkVertex, linkFragment } ); // push
	coll.updateShaders			({ linkVertex, linkFragment }, { dsLayout });

	coll.buildAndVerify();

	ZShaderObject				vertexShaderObject		= coll.getShader(VK_SHADER_STAGE_VERTEX_BIT, (params.useLinkedShaders ? 1 : 0));
	ZShaderObject				fragmentShaderObject	= coll.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, (params.useLinkedShaders ? 1 : 0));

	std::cout
		<< coll.getShaderFile(VK_SHADER_STAGE_VERTEX_BIT, 0) << std::endl
		<< coll.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 0) << std::endl
		<< coll.getShaderFile(VK_SHADER_STAGE_VERTEX_BIT, 1) << std::endl
		<< coll.getShaderFile(VK_SHADER_STAGE_FRAGMENT_BIT, 1) << std::endl;

	VertexInput					vertexInput			(cs.device);
	{
		std::vector<Vec2>		vertices			{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
		std::vector<Vec3>		colors				{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		vertexInput.binding(0).addAttributes(vertices, colors);
	}

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});

	ZPipelineLayout				pipelineLayout		= lm.createPipelineLayout({ dsLayout }); // push

	int drawTrigger = 1;
	cs.events().cbKey.set(onKey, nullptr);
	cs.events().cbWindowSize.set(onResize, &drawTrigger);

	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		if (params.infinity) { drawTrigger = 1; }

		const uint32_t bufferSize = bufferGetElementCount<uint32_t>(testBuffer);
		static std::vector<uint32_t> bufferData(bufferSize);
		bufferRead(testBuffer, bufferData);
		std::cout << bufferData[0] << bufferData[1] << std::endl;
	};

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		ZImage image = framebufferGetImage(framebuffer);
		add_cref<VkExtent3D> size = imageGetExtent(image);
		ZImageMemoryBarrier makeImageGeneral(image,	VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_GENERAL);

		commandBufferBegin(cmdBuffer);
			commandBufferBindDescriptorSets(cmdBuffer, pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS);
			commandBufferBindShaders(cmdBuffer, { vertexShaderObject, fragmentShaderObject });

			commandBufferSetDefaultDynamicStates(cmdBuffer, vertexInput, swapchain.viewport, &swapchain.scissor);

			commandBufferPipelineBarriers(cmdBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, makeImageGeneral);

			commandBufferBeginRendering(cmdBuffer, size.width, size.height,
										{ framebufferGetView(framebuffer) }, { { clearColor } });
				di.vkCmdDraw(*cmdBuffer, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferEndRendering(cmdBuffer);

			commandBufferMakeImagePresentationReady(cmdBuffer, image);
		commandBufferEnd(cmdBuffer);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(drawTrigger),
		Canvas::OnIdle(), std::bind(onAfterRecording, std::ref(cs)));
}

TriLogicInt runTriangleMultipleThreads (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<TestParams> params)
{
	std::vector<ZQueue> queues	= buildQueues(cs.device, params.threadCount);
	if (!verifyQueues(queues))
	{
		std::cout << "[ERROR} Unable to test on threads due to lack of hardware threads." << std::endl;
		return (1);
	}
	UNREF(assets);
	std::cout << "Currently this variant is experimental phase" << std::endl;
	return {};

	/*
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

	cs.events().cbKey.set(onKey, nullptr);
	cs.events().cbWindowSize.set(onResize, nullptr);

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
	auto threadsData = createVector(params.threadCount, createThreadLocal);

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
	*/
}

} // unnamed namespace

template<> struct TestRecorder<SHADER_OBJECT_TRIANGLE>
{
	static bool record (TestRecord&);
};
bool TestRecorder<SHADER_OBJECT_TRIANGLE>::record (TestRecord& record)
{
	record.name = "shader_object_triangle";
	record.call = &prepareTests;
	return true;
}
