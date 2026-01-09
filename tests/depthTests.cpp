#include "depthTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfVertexInput.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfMatrix.hpp"

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	bool buildAlways = false;
	float clearDepth = 0.5f;
	Params(add_cref<std::string> assets_)
		: assets(assets_)
	{
	}
	OptionParser<Params> getParser();
};
constexpr Option optionBuildAlways{ "--build-always", 0 };
constexpr Option optionClearDepth{ "-clear", 1 };
OptionParser<Params> Params::getParser()
{
	OptionFlags				flagsDef(OptionFlag::PrintValueAsDefault);
	add_ref<Params>			params = *this;
	OptionParser<Params>	parser(params);

	parser.addOption(&Params::buildAlways, optionBuildAlways,
		"Rebuild the shaders each time you run application", { false }, flagsDef)
		->setParamName("buildAlways");
	parser.addOption(&Params::clearDepth, optionClearDepth,
		"Depth clear value", { 0.5f }, flagsDef)->setParamName("clearDepth");

	return parser;
}

TriLogicInt runTests(add_ref<Canvas> cs, add_cref<Params> params);
TriLogicInt prepareTests(add_cref<TestRecord> record, add_cref<strings> cmdLineParams);

TriLogicInt prepareTests(add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	add_cref<GlobalAppFlags> gf(getGlobalAppFlags());

	Params params(record.assets);
	OptionParser<Params> parser = params.getParser();
	parser.parse(cmdLine);
	OptionParserState state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout, 60);
		return {};
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messages.str() << std::endl;
		if (state.hasErrors) return {};
	}

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps)
	{
		caps.addUpdateFeatureIf(&VkPhysicalDeviceSynchronization2Features::synchronization2)
			.checkSupported("synchronization2");
		caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).checkSupported();

		caps.addUpdateFeatureIf(&VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT::mutableDescriptorType)
			.checkSupported("mutableDescriptorType");
		caps.addExtension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME).checkSupported();
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, strings(), strings(), canvasStyle, onEnablingFeatures, gf.apiVer);

	return runTests(cs, params);
}

namespace ev
{

struct MVP
{
	glm::mat4 model;
	MVP () : model(glm::mat4(1.0f)) {}
	MVP (add_cref<glm::mat4> rotate) : model(rotate) {}
};

struct UserData
{
	int drawTrigger = 1;
	double xCursor = 0.0;
	double yCursor = 0.0;
	double miceKeyXcursor = 0.0;
	double miceKeyYcursor = 0.0;
	bool ctrlPressed = false;
	bool leftCtrlPressed = false;
	bool rightCtrlPressed = false;
	bool shiftPressed = false;
	float xRotate = 0.0f;
	float yRotate = 0.0f;
	ZBuffer matrixBuffer;
	UserData (ZBuffer matrixBuffer_)
		: matrixBuffer(matrixBuffer_)
	{
		bufferWrite(matrixBuffer, MVP());
	}
};

void updateMatrices (add_ref<Canvas>, add_cref<UserData> ui)
{
	glm::quat rotX = glm::angleAxis(ui.xRotate, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotY = glm::angleAxis(ui.yRotate, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 rotYX = glm::mat4_cast(rotY * rotX);

	MVP mvp(rotYX);
	bufferWrite(ui.matrixBuffer, mvp);
}

void onKey (add_ref<Canvas> cs, add_ptr<void> userData, const int key, int scancode, int action, int mods)
{
	MULTI_UNREF(scancode, mods);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
	{
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->ctrlPressed = (GLFW_PRESS == action || GLFW_REPEAT == action);
		if (key == GLFW_KEY_LEFT_CONTROL)
			ui->leftCtrlPressed = ui->ctrlPressed;
		else ui->rightCtrlPressed = ui->ctrlPressed;
	}
	if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
	{
		ui->shiftPressed = (GLFW_PRESS == action || GLFW_REPEAT == action);
	}
}

void onCursorPos(add_ref<Canvas>, void* userData, double xpos, double ypos)
{
	auto ui = static_cast<add_ptr<UserData>>(userData);
	++ui->drawTrigger;

	ui->xCursor = xpos;
	ui->yCursor = ypos;

	const double wx = ui->miceKeyXcursor - xpos;
	const double wy = ypos - ui->miceKeyYcursor;

	if (ui->ctrlPressed)
	{
		//const double dx = wx / double(canvas.width);
		//const double dy = wy / double(canvas.height);

		const float u = 1.0f / 128.0f;

		ui->yRotate += wx > 0.0 ? u : wx < 0.0 ? (-u) : 0.0f;
		ui->xRotate += wy > 0.0 ? u : wy < 0.0 ? (-u) : 0.0f;
	}

	ui->miceKeyXcursor = xpos;
	ui->miceKeyYcursor = ypos;
}

} // namespace ev

void prepareVertices(add_ref<VertexInput> vertexInput, add_cref<Params>)
{
	const float z = 0.6f;
	std::vector<Vec3>		vertices{ { -0.5, -0.5, z }, { +.5, 0, z }, { -0.5, +0.5, z } };
	vertexInput.binding(0).addAttributes(vertices);
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule> buildProgram(ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "vertex.shader");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment0.shader");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment1.shader");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer,
		flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);

	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 0u),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT, 1u),
	};
}

TriLogicInt runTests(add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_cref<ZDeviceInterface>	di(canvas.device.getInterface());
	LayoutManager				lm(canvas.device);
	VertexInput					vertexInput(canvas.device);
	prepareVertices(vertexInput, params);

	ZShaderModule				vertex;
	ZShaderModule				fragment0, fragment1;
	std::tie(vertex, fragment0, fragment1)	= buildProgram(canvas.device, params);

	const VkFormat				colorFormat = canvas.surfaceFormat;
	const VkClearValue			clearColor{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const VkFormat				depthFormat = formatSelectMaxDepthSupported(canvas.device);
	const VkClearValue			clearDepth{ { { params.clearDepth, 0u } } };
	const std::vector<RPA>		colors{ RPA(AttachmentDesc::Presentation, colorFormat, clearColor,
															VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
										RPA(AttachmentDesc::DeptStencil, depthFormat, clearDepth,
															VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
															VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) };
	const ZAttachmentPool		attachmentPool(colors);
	const ZSubpassDescription2	subpass0({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
											RPAR(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) });
	const ZSubpassDescription2	subpass1({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
											RPAR(1u, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, AttachmentDesc::Input) });
	const ZSubpassDependency2	dep01(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
									VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
	ZRenderPass					renderPass = createRenderPass(canvas.device, attachmentPool, subpass0, dep01, subpass1);
	const VkShaderStageFlags	shaderStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
	const uint32_t				bindColor = lm.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									shaderStages, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });
	const uint32_t				bindDepth = lm.addBinding(VK_DESCRIPTOR_TYPE_MUTABLE_EXT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
									shaderStages, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
	ZBuffer						mvpBuffer = createBuffer<ev::MVP>(canvas.device, 1u, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const uint32_t				bindMatrix = lm.addBinding(mvpBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStages);
	ZBuffer						resBuffer = createBuffer<uint32_t>(canvas.device, 16u);
	const uint32_t				bindResult = lm.addBinding(resBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStages);
	ZDescriptorSetLayout		dsLayout = lm.createDescriptorSetLayout(false);
	ZDescriptorSet				dsSet = lm.getDescriptorSet(dsLayout);
	struct PC {
		float clear;
	}							pc{ params.clearDepth };
	ZPushRange<PC>				pushRange(shaderStages);
	ZPipelineLayout				pipelineLayout = lm.createPipelineLayout({ dsLayout }, pushRange);
	ZPipeline					pipeline0 = createGraphicsPipeline(pipelineLayout, renderPass,
																	vertexInput, vertex, fragment0, gpp::SubpassIndex(0),
																	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
																	gpp::DepthTestEnable(true), gpp::DepthWriteEnable(true),
																	VK_COMPARE_OP_GREATER_OR_EQUAL);
	ZPipeline					pipeline1 = createGraphicsPipeline(pipelineLayout, renderPass,
																	vertexInput, vertex, fragment1, gpp::SubpassIndex(1),
																	VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	ZCommandBuffer				auxCmd = createCommandBuffer(createCommandPool(canvas.device, canvas.graphicsQueue));

	ev::UserData userData(mvpBuffer);
	canvas.events().cbKey.set(ev::onKey, &userData);
	canvas.events().cbCursorPos.set(ev::onCursorPos, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> sc,
									ZCommandBuffer cmd, ZFramebuffer fb)
	{
		ZImageView colorView = framebufferGetView(fb, 0);
		ZImageView depthView = framebufferGetView(fb, 1);
		ZImage colorImage = imageViewGetImage(colorView); UNREF(colorImage);
		ZImage depthImage = imageViewGetImage(depthView); UNREF(depthImage);

		ZImageMemoryBarrier colorBarrier(colorImage, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_GENERAL);
		ZImageMemoryBarrier depthBarrier(depthImage, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_IMAGE_LAYOUT_GENERAL);

		{
			commandBufferBegin(auxCmd);
			commandBufferPipelineBarriers(auxCmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, colorBarrier, depthBarrier);
			commandBufferEnd(auxCmd);
			commandBufferSubmitAndWait(auxCmd);
		}

		lm.updateDescriptorSet(dsSet, bindColor, colorView, VK_IMAGE_LAYOUT_MAX_ENUM, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		lm.updateDescriptorSet(dsSet, bindDepth, depthView, VK_IMAGE_LAYOUT_MAX_ENUM, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		lm.updateDescriptorSet(dsSet, bindMatrix, mvpBuffer);
		lm.updateDescriptorSet(dsSet, bindResult, resBuffer);

		commandBufferBegin(cmd);
		commandBufferBindVertexBuffers(cmd, vertexInput);
		commandBufferPushConstants(cmd, pipelineLayout, pc);
		di.vkCmdSetViewport(*cmd, 0, 1, &sc.viewport);
		di.vkCmdSetScissor(*cmd, 0, 1, &sc.scissor);
			auto rpbi = commandBufferBeginRenderPass(cmd, fb);
				commandBufferBindPipeline(cmd, pipeline0);
				di.vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferNextSubpass(rpbi);
				commandBufferBindPipeline(cmd, pipeline1);
				di.vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
		commandBufferEnd(cmd);
	};

	auto onAfterRecording = [&](add_ref<Canvas> cs)
	{
		ev::updateMatrices(cs, userData);
	};

	return canvas.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger), Canvas::OnIdle(), onAfterRecording);
}

} // unnamed namespace

template<> struct TestRecorder<DEPTH>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DEPTH>::record(TestRecord& record)
{
	record.name = "depth";
	record.call = &prepareTests;
	return true;
}
