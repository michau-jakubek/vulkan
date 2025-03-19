#include "cogWheelsTests.hpp"
#include "vtfCogwheelTools.hpp"
#include "vtfOptionParser.hpp"
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
#include "vtfCopyUtils.hpp"
#include "vtfTemplateUtils.hpp"

#include <iostream>
#include <vector>
#include <cmath>

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	VkPrimitiveTopology topology;
	bool geometryRequired;
	bool buildAlways;
	Params (add_cref<std::string> assets_)
		: assets(assets_)
		, topology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
		, geometryRequired(false)
		, buildAlways(true)
	{
	}
};

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);

	Params params(record.assets);

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	bool depthUnrestricted = false;
	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps) -> void
	{
		//if (params.geometryRequired || needsTopologyGeometry(params.topology))
		//{
		//	if (!caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::geometryShader))
		//		throw std::runtime_error("[ERROR] VkPhysicalDeviceFeatures.geometryShader not supported by device");
		//}

		if (!caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::pipelineStatisticsQuery))
			throw std::runtime_error("[ERROR] VkPhysicalDeviceFeatures::pipelineStatisticsQuery not supported by device");

		//if (isTopologyList(params.topology))
		{
			caps.requiredExtension.emplace_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
			if (!caps.addUpdateFeatureIf(&VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT
											::primitiveTopologyListRestart))
				throw std::runtime_error("[ERROR] primitiveTopologyListRestart not supported by device");
		}

		//if (containsString(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME, caps.availableExtensions))
		//{
		//	depthUnrestricted = true;
		//	caps.requiredExtension.emplace_back(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
		//}
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.minDepth = depthUnrestricted ? 0.0f : 0.0f;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	if (false == depthUnrestricted)
	{
		std::cout << VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME << " not available" << std::endl;
	}

	return runTests(cs, params);
}

struct MVP
{
	glm::mat4 model1;
	glm::mat4 model2;
	glm::mat4 view;
	glm::mat4 proj;
	MVP() {
		const float scale = 1.0f / 50.0f;

		auto m = glm::mat4(1.0f);
		m = glm::scale(m, glm::vec3(scale, scale, 1.0f));
		m = glm::translate(m, glm::vec3(-25.2f, 0.0f, 0.0f));
		m = glm::rotate(m, 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
		model1 = m;

		m = glm::mat4(1.0f);
		m = glm::scale(m, glm::vec3(scale, scale, 1.0f));
		m = glm::translate(m, glm::vec3(+25.2f, 0.0f, 0.0f));
		m = glm::rotate(m, 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
		model2 = m;

		glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
		glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

		proj = glm::mat4(1.0f);
	}
	MVP(add_ref<Canvas> cs) : MVP() {
		const float fov = glm::radians(45.0f);
		const float aspectRatio = float(cs.width) / float(cs.height);
		const float nearPlane = 0.1f;
		const float farPlane = 100.0f;
		proj = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	}
};

struct UserData
{
	int drawTrigger = 1;
	bool paused = false;
	uint64_t queryResult[2];
	ZBuffer mvpBuffer;
	UserData(ZBuffer mvpBuffer_) : mvpBuffer(mvpBuffer_)
	{
		MVP mvp;
		bufferWrite(mvpBuffer, mvp);
	}
};

void updateMatrices (add_ref<Canvas> cs, ZBuffer buffer)
{
	static std::mutex mtx;

	const float rotateStep = 1.0f / 64.f;
	static float rotateNext = 0.0f;
	rotateNext += rotateStep;

	if (rotateNext > glm::two_pi<float>())
	{
		rotateNext = 0.0f;
	}

	std::lock_guard<std::mutex> lock(mtx);
	MVP mvp(cs);
	mvp.model1 = glm::rotate(mvp.model1, (+rotateNext), glm::vec3(0.0f, 0.0f, 1.0f)),
	mvp.model2 = glm::rotate(mvp.model2, (-rotateNext), glm::vec3(0.0f, 0.0f, 1.0f)),
	bufferWrite(buffer, mvp);
}

void onKey (add_ref<Canvas> cs, add_ptr<void> userData, const int key, int scancode, int action, int mods)
{
	MULTI_UNREF(scancode, mods);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		std::cout << "Vertices: " << ui->queryResult[0] << ", Primitives: " << ui->queryResult[1] << std::endl;
	}
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		ui->paused ^= true;
	}
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule>
buildProgram (ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "vertex.shader");
	const bool needsGeometry = false;
	//params.geometryRequired || needsTopologyGeometry(params.topology);
	//if (needsGeometry)
	//{
	//	switch (params.topology)
	//	{
	//	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
	//		programs.addFromFile(VK_SHADER_STAGE_GEOMETRY_BIT, "lines_strip_with_adjacency.shader");
	//		break;
	//	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
	//		programs.addFromFile(VK_SHADER_STAGE_GEOMETRY_BIT, "triangle_list.shader");
	//		break;
	//	default:
	//		ASSERTFALSE("Unknown topology: ", "");
	//	}
	//	
	//}
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment.shader");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer,
							flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);

	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		needsGeometry ? programs.getShader(VK_SHADER_STAGE_GEOMETRY_BIT) : ZShaderModule(),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
	};
}

std::vector<Vec4> generateGearOutline (VkPrimitiveTopology topology)
{
	CogwheelDescription cd{};
	cd.D = 50.0f;
	cd.Ha = 2.0f;
	cd.Hf = 2.2f;
	cd.Z = 20u;
	const std::vector<Vec2> cw = makeCogwheel(cd, topology);
	std::vector<Vec4> outline(cw.size());
	std::transform(cw.begin(), cw.end(), outline.begin(),
		[](add_cref<Vec2> src) { return src.cast<Vec4>(0.0f, 1.0f); });
	return outline;
}

std::tuple<ZBuffer, ZBuffer, uint32_t>
createVertexAndIndexBuffers (add_ref<VertexInput> input, add_cref<Params> params)
{
	ZBuffer indices;
	uint32_t indexCount = 0;

	// Generowanie obwiedni
	std::vector<Vec4> gearOutline = generateGearOutline(params.topology);

	input.binding(0).addAttributes(gearOutline);

	indexCount = data_count(gearOutline);
	indices = createIndexBuffer(input.device, indexCount, VK_INDEX_TYPE_UINT32);
	BufferTexelAccess<uint32_t> a(indices, indexCount, 1u, 1u);
	for (uint32_t i = 0u; i < indexCount; ++i)
		a.at(i, 0) = i;

	return { input.binding(0).getBuffer(), indices, indexCount };
}

TriLogicInt runTests (Canvas& cs, add_cref<Params> params)
{
	VertexInput vertexInput(cs.device);
	uint32_t indexCount = 0;
	ZBuffer vertexBuffer, indexBuffer;
	std::tie(vertexBuffer, indexBuffer, indexCount) = createVertexAndIndexBuffers(vertexInput, params);

	ZShaderModule vert, geom, frag;
	std::tie(vert, geom, frag) = buildProgram(cs.device, params);

	const VkFormat		format = cs.surfaceFormat;
	const VkClearValue	clearColor{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };

	LayoutManager		lm(cs.device);
	const uint32_t		mvpBinding = lm.addBinding<MVP>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	ZDescriptorSetLayout dsLayout = lm.createDescriptorSetLayout();
	ZBuffer				mvpBuffer = std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(mvpBinding)).buffer;
	ZPipelineLayout		pipelineLayout = lm.createPipelineLayout({ dsLayout });
	ZRenderPass			renderPass = createColorRenderPass(cs.device, { format }, { {clearColor} });
	ZPipeline			pipeline = createGraphicsPipeline(pipelineLayout, renderPass,
															vert, geom, frag, params.topology, vertexInput.binding(0),
															gpp::SubpassIndex(0), gpp::PrimitiveRestart(true),
															gpp::DepthTestEnable(true),
															VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	const ZQueryPool	queryPool = createQueryPool(cs.device, VK_QUERY_TYPE_PIPELINE_STATISTICS,
													VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
													| VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT);
	ZBuffer				queryResults = createBuffer<uint64_t[2]>(cs.device);

	UserData userData(mvpBuffer);
	cs.events().cbKey.set(onKey, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> sc, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		commandBufferBegin(cmdBuffer);
		commandBufferBindPipeline(cmdBuffer, pipeline);
		commandBufferBindVertexBuffers(cmdBuffer, vertexInput, { vertexBuffer });
		commandBufferBindIndexBuffer(cmdBuffer, indexBuffer);
		commandBufferSetScissor(cmdBuffer, sc);
		commandBufferSetViewport(cmdBuffer, sc);
		commandBufferResetQueryPool(cmdBuffer, queryPool);

		auto qpbi = commandBufferBeginQuery(cmdBuffer, queryPool, 0);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer, 0u);
				vkCmdDrawIndexed(*cmdBuffer, indexCount, 1, 0, 0, 0);
				vkCmdDrawIndexed(*cmdBuffer, indexCount, 1, 0, 0, 1);
			commandBufferEndRenderPass(rpbi);
		commandBufferEndQuery(qpbi);

		vkCmdCopyQueryPoolResults(*cmdBuffer, *queryPool, 0, 1, *queryResults, 0, sizeof(uint64_t),
									VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

		commandBufferMakeImagePresentationReady(cmdBuffer, framebufferGetImage(framebuffer));
		commandBufferEnd(cmdBuffer);
	};

	FPS fps([caretBack = std::string(16, '\x8')](float fps, float totalTime, float bestFPS, float worstFPS)
	{
		UNREF(totalTime), UNREF(bestFPS), UNREF(worstFPS);
		std::cout << caretBack << "FPS: " << fps;
	});

	auto stableRedrawCall = [&](float, float, float, float)
	{
		if (false == userData.paused)
		{
			updateMatrices(cs, mvpBuffer);
		}
	};
	FPS stableRedraw(stableRedrawCall, (1.0f / 64.0f));

	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		bufferRead(queryResults, userData.queryResult);
		userData.drawTrigger += 1;
		stableRedraw.touch();
		fps.touch();
	};

	return cs.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger), {}, onAfterRecording);
}

} // unnamed namespace

template<> struct TestRecorder<COGWHEELS>
{
	static bool record(TestRecord&);
};
bool TestRecorder<COGWHEELS>::record(TestRecord& record)
{
	record.name = "cogwheels";
	record.call = &prepareTests;
	return true;
}

