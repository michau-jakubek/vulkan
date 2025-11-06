#include "cogWheelsTests.hpp"
#include "vtfCogwheelTools.hpp"
#include "vtfOptionParser.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfZImage.hpp"
#include "vtfDSBMgr.hpp"
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
#include "vtfOptionParser.hpp"

#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>

namespace
{
using namespace vtf;

struct Params
{
	add_cref<std::string> assets;
	VkPrimitiveTopology topology;
	bool geometryRequired;
	bool buildAlways;
	bool enableFPS;
	Params (add_cref<std::string> assets_)
		: assets(assets_)
		, topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		, geometryRequired(false)
		, buildAlways(false)
		, enableFPS(false)
	{
	}
	OptionParser<Params> getParser ();
	void printHelp (add_cref<Params> defaultValue, add_ref<std::ostream> str) const;
};
constexpr Option optionFPS("-fps", 0);
OptionParser<Params> Params::getParser ()
{
	OptionFlags          flags(OptionFlag::PrintDefault);
	OptionParser<Params> parser(*this);

	parser.addOption(&Params::enableFPS, optionFPS, "Print FPS", { enableFPS }, flags);

	return parser;
}
void Params::printHelp (add_cref<Params> defaultValue, add_ref<std::ostream> str) const
{
	Params def(defaultValue);
	auto parser = def.getParser();
	str << "Navigation:\n"
		"  Esc         - quit this app immediately\n"
		"  Space       - pause/run\n"
		"  Scroll      - zoom in/out\n"
		"  Shift+Mouse - move scene\n"
		"  Ctrl+Mouse  - rotate X & Y\n"
		"  S           - print statistics\n"
		"Parameters:\n";
	parser.printOptions(std::cout, 70);
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	UNREF(cmdLineParams);

	Params params(record.assets);
	auto parser = params.getParser();
	auto unresolvedParams = parser.parse(cmdLineParams); UNREF(unresolvedParams);
	OptionParserState state = parser.getState();
	if (state.hasErrors)
	{
		std::cout << state.messagesText() << std::endl;
		return {};
	}
	if (state.hasHelp)
	{
		params.printHelp(Params(record.assets), std::cout);
		return {};
	}


	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	auto onEnablingFeatures = [&](add_ref<DeviceCaps> caps) -> void
	{
		if (!caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::pipelineStatisticsQuery))
			throw std::runtime_error("[ERROR] VkPhysicalDeviceFeatures::pipelineStatisticsQuery not supported by device");

		if (false)
		{
			caps.requiredExtension.emplace_back(VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME);
			if (!caps.addUpdateFeatureIf(&VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT
											::primitiveTopologyListRestart))
				throw std::runtime_error("[ERROR] primitiveTopologyListRestart not supported by device");
		}
	};

	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.acquirableImageCount = 10;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.apiVer, gf.debugPrintfEnabled);

	return runTests(cs, params);
}

struct MVP
{
	glm::mat4 model1;
	glm::mat4 model2;
	glm::mat4 model3;
	glm::mat4 model4;
	glm::mat4 view;
	glm::mat4 proj;
	MVP() {
		const float scale = 1.0f / 50.0f;

		auto initModel = [&](add_ref<glm::mat4> model)
		{
			auto m = glm::mat4(1.0f);
			m = glm::scale(m, glm::vec3(scale, scale, scale));
			//m = glm::translate(m, glm::vec3(-25.2f, 0.0f, 0.0f));
			//m = glm::rotate(m, 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
			model = m;
		};

		initModel(model1);
		initModel(model2);
		initModel(model3);
		initModel(model4);

		glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
		glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, -5.0f);
		glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

		proj = glm::mat4(1.0f);
	}
	MVP(add_ref<Canvas> cs, float cameraZ) : MVP() {
		glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, cameraZ);
		glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, -5.0f);
		glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

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
	double xCursor = 0.0;
	double yCursor = 0.0;
	double miceKeyXcursor = 0.0;
	double miceKeyYcursor = 0.0;
	bool paused = false;
	bool ctrlPressed = false;
	bool leftCtrlPressed = false;
	bool rightCtrlPressed = false;
	bool micePressed = false;
	bool leftMicePressed = false;
	bool rightMicePressed = false;
	bool shiftPressed = false;
	bool zRotateMode = false;
	float xRotate = 0.0f;
	float yRotate = 0.0f;
	float zRotate = 0.0f;
	float cameraZ = 3.0f;
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float rotateStep = 1.0f / 64.0f;
	float rotOffset = 0.0f;
	float rotateTrigger = 1.0f / 128.0f;
	uint64_t queryResult[2];
	ZBuffer mvpBuffer;
	UserData(ZBuffer mvpBuffer_) : mvpBuffer(mvpBuffer_)
	{
		MVP mvp;
		bufferWrite(mvpBuffer, mvp);
	}
};

void updateMatrices (add_ref<Canvas> cs, add_cref<UserData> ui)
{
	const float rotateStep = 1.0f / 256.f;
	static float bigRotate = 0.0f;
	static float smallRotate = 0.0f;

	bigRotate += rotateStep;
	smallRotate += rotateStep * 2.0f;

	if (bigRotate > glm::two_pi<float>())
	{
		bigRotate = 0.0f;
	}

	if (smallRotate > glm::two_pi<float>())
	{
		smallRotate = 0.0f;
	}

	glm::quat rotX = glm::angleAxis(ui.xRotate, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rotY = glm::angleAxis(ui.yRotate, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 rotYX = glm::mat4_cast(rotY * rotX);

	MVP mvp(cs, ui.cameraZ);

	mvp.model1 *= rotYX;
	mvp.model1 = glm::translate(mvp.model1, glm::vec3(0.0f, 0.0f, 0.0f));
	mvp.model1 = glm::translate(mvp.model1, glm::vec3(ui.offsetX, ui.offsetY, 0.0f));
	mvp.model1 = glm::rotate(mvp.model1, ui.zRotate - bigRotate, glm::vec3(0.0f, 0.0f, 1.0f));


	mvp.model2 *= rotYX;
	const float rotCorrect2 = 0.17f;
	const Vec2 start2 = rotate(Vec2(37.5f, 0), glm::pi<float>() * 0.75f);
	mvp.model2 = glm::translate(mvp.model2, glm::vec3(start2.x(), start2.y(), 0.0f));
	mvp.model2 = glm::translate(mvp.model2, glm::vec3(ui.offsetX, ui.offsetY, 0.0f));
	mvp.model2 = glm::rotate(mvp.model2, smallRotate + rotCorrect2 + ui.zRotate, glm::vec3(0.0f, 0.0f, 1.0f));

	mvp.model3 *= rotYX;
	const float rotCorrect3 = 0.5f;
	const Vec2 start3 = rotate(Vec2(37.5f, 0), glm::pi<float>() * 0.25f);
	mvp.model3 = glm::translate(mvp.model3, glm::vec3(start3.x(), start3.y(), 0.0f));
	mvp.model3 = glm::translate(mvp.model3, glm::vec3(ui.offsetX, ui.offsetY, 0.0f));
	mvp.model3 = glm::rotate(mvp.model3, smallRotate + rotCorrect3 + ui.zRotate, glm::vec3(0.0f, 0.0f, 1.0f));

	mvp.model4 *= rotYX;
	const Vec2 start4(0, -37.5f);
	const float rotCorrect4 = 0.335f;
	mvp.model4 = glm::translate(mvp.model4, glm::vec3(start4.x(), start4.y(), 0.0f));
	mvp.model4 = glm::translate(mvp.model4, glm::vec3(ui.offsetX, ui.offsetY, 0.0f));
	mvp.model4 = glm::rotate(mvp.model4, smallRotate + rotCorrect4 + ui.zRotate, glm::vec3(0.0f, 0.0f, 1.0f));

	bufferWrite(ui.mvpBuffer, mvp);
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
	if (key == GLFW_KEY_LEFT_SHIFT|| key == GLFW_KEY_RIGHT_SHIFT)
	{
		ui->shiftPressed = (GLFW_PRESS == action || GLFW_REPEAT == action);
	}
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
	{
		//typedef routine_arg_t<decltype(std::setprecision), 0> prec_t;
		//const prec_t prec = static_cast<prec_t>(std::cout.precision());
		std::cout << "Vertices: " << ui->queryResult[0]
			<< ", Primitives: " << ui->queryResult[1]
			//<< ", K: " << std::fixed << std::setprecision(10) << ui->rotOffset << std::setprecision(prec) << std::defaultfloat
			<< std::endl;
	}
	if (key == GLFW_KEY_K && action == GLFW_PRESS)
	{
		ui->rotOffset += 0.01f;
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		ui->rotOffset = 0.0;
	}
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		ui->paused ^= true;
	}
	if ((key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) && action == GLFW_PRESS)
	{
	}
	if ((key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) && action == GLFW_PRESS)
	{
	}
}

void onMouseBtn (add_ref<Canvas>, void* userData, int button, int action, int mods)
{
	UNREF(mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		auto ui = static_cast<add_ptr<UserData>>(userData);
		++ui->drawTrigger;
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->micePressed = (action == GLFW_PRESS);
		if (button == GLFW_MOUSE_BUTTON_LEFT)
			ui->leftMicePressed = ui->micePressed;
		else ui->rightMicePressed = ui->micePressed;
	}
}

void onCursorPos (add_ref<Canvas>, void* userData, double xpos, double ypos)
{
	auto ui = static_cast<add_ptr<UserData>>(userData);
	++ui->drawTrigger;

	ui->xCursor = xpos;
	ui->yCursor = ypos;

	const double wx = ui->miceKeyXcursor - xpos;
	const double wy = ypos - ui->miceKeyYcursor;

	if (ui->shiftPressed)
	{
		const float d = 0.25f;
		ui->offsetX += wx > 0.0 ? (-d) : wx < 0.0 ? (+d) : 0.0f;
		ui->offsetY += wy > 0.0 ? (+d) : wy < 0.0 ? (-d) : 0.0f;
	}
	if (ui->micePressed || ui->ctrlPressed)
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

void onScroll (add_ref<Canvas>, void* userData, double xScrollOffset, double yScrollOffset)
{
	UNREF(xScrollOffset);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	++ui->drawTrigger;
	if (ui->ctrlPressed)
	{
	}
	else
	{
		const double delta = 0.0625f;
		ui->cameraZ = std::clamp(float(std::copysign(delta, yScrollOffset) + ui->cameraZ), 0.5f, 10.0f);
	}
}

std::tuple<ZShaderModule, ZShaderModule, ZShaderModule>
buildProgram (ZDevice device, add_cref<Params> params)
{
	ProgramCollection			programs(device, params.assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "vertex.shader");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment.shader");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer,
							flags.spirvValidate, flags.genSpirvDisassembly, params.buildAlways);

	return
	{
		programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		ZShaderModule(),
		programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT),
	};
}

struct GearOutlineInfo
{
	std::pair<uint32_t, uint32_t> front;
	std::pair<uint32_t, uint32_t> back;
	std::pair<uint32_t, uint32_t> surf;
};

template<class X>
void copyByStaging(const std::vector<X>& src, ZBuffer dst, uint32_t startIndex, ZQueue queue, uint32_t chunkSize = 1048576u)
{
	static_assert(std::is_reference_v<decltype(src)>, "???");
	const uint32_t chunkOptimalSize = ROUNDUP(chunkSize, uint32_t(sizeof(X)));
	const uint32_t elemsPerChunk = chunkOptimalSize / sizeof(X);
	const uint32_t numChunks = uint32_t(data_byte_length(src) / chunkOptimalSize);
	const uint32_t dstOffset = uint32_t(startIndex * sizeof(X));
	const auto dstSize = bufferGetSize(dst);

	ZDevice device = dst.getParam<ZDevice>();
	ZBuffer staging = createBuffer<X>(device, elemsPerChunk);
	ZBufferMemoryBarrier host(staging, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

	for (uint32_t chunk = 0u, srcIndex = 0u; chunk < numChunks; ++chunk, srcIndex += elemsPerChunk)
	{
		ASSERTION(((chunk + 1u) * chunkOptimalSize + dstOffset) <= dstSize);
		bufferWrite(staging, src, 0u, srcIndex, elemsPerChunk);
		//commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, host);

		VkBufferCopy copy{};
		copy.srcOffset = 0u;
		copy.dstOffset = chunk * chunkOptimalSize + dstOffset;
		copy.size = chunkOptimalSize;
		OneShotCommandBuffer cmd(device, queue);
		vkCmdCopyBuffer(*(ZCommandBuffer)cmd, *staging, *dst, 1u, &copy);
	}

	const uint32_t chunkRestSize = uint32_t(data_byte_length(src) % chunkOptimalSize);
	if (chunkRestSize)
	{
		ASSERTION((numChunks * chunkOptimalSize + chunkRestSize + dstOffset) <= dstSize);
		bufferWrite(staging, src, 0u, (numChunks * elemsPerChunk), (chunkRestSize / sizeof(X)));
		//commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, host);

		VkBufferCopy copy{};
		copy.srcOffset = 0u;
		copy.dstOffset = numChunks * chunkOptimalSize + dstOffset;
		copy.size = chunkRestSize;
		OneShotCommandBuffer cmd(device, queue);
		vkCmdCopyBuffer(*(ZCommandBuffer)cmd, *staging, *dst, 1u, &copy);
	}
}

typedef VecX<float, 7> Vec7;
std::vector<Vec7> resizeWithNormals (add_cref<std::vector<Vec4>> srcTriangleList)
{
	const uint32_t vertexCount = data_count(srcTriangleList);
	ASSERTION(vertexCount % 3 == 0);
	std::vector<Vec7> out(vertexCount);
	for (uint32_t v = 0; v < vertexCount; v += 3u)
	{
		add_cref<Vec4> a(srcTriangleList[v]);
		add_cref<Vec4> b(srcTriangleList[v + 1]);
		add_cref<Vec4> c(srcTriangleList[v + 2]);

		const Vec4 ab = a - b;
		const Vec4 ac = a - c;
		Vec3 cross = ab.cast<Vec3>().cross(ac.cast<Vec3>());
		out[v] = a.cast<VecX<float, 7>>(cross.x(), cross.y(), cross.z());
		out[v + 1u] = b.cast<VecX<float, 7>>(cross.x(), cross.y(), cross.z());
		out[v + 2u] = c.cast<VecX<float, 7>>(cross.x(), cross.y(), cross.z());
	}
	return out;
}

std::tuple<ZBuffer, ZBuffer, GearOutlineInfo, GearOutlineInfo>
createVertexAndIndexBuffers (add_ref<VertexInput> input, add_cref<Params> params, ZQueue queue)
{
	UNREF(params);

	const VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	CogwheelDescription cdBig{};
	cdBig.mainDiameter = 50.0f;
	cdBig.toothHeadHeight = 2.0f;
	cdBig.toothFootHeight = 2.2f;
	cdBig.toothCount = 20u;
	cdBig.toothToSpaceFactor = 30.0f / 64.0f;
	cdBig.angleStep = 0.01f;

	CogwheelDescription cdSmall{};
	cdSmall.mainDiameter = 25.0f;
	cdSmall.toothHeadHeight = 2.0f;
	cdSmall.toothFootHeight = 2.2f;
	cdSmall.toothCount = 10u;
	cdSmall.toothToSpaceFactor = 30.0f / 64.0f;
	cdSmall.angleStep = 0.01f;

	const std::vector<Vec2> outBig = generateCogwheelOutlinePoints(cdBig);
	const std::vector<Vec2> outSmall = generateCogwheelOutlinePoints(cdSmall);

	uint32_t vertexCount = 0;

	vertexCount += data_count(outBig) * 4 * 3;
	vertexCount += data_count(outSmall) * 4 * 3;

	ZBuffer vertices = createBuffer<Vec7>(input.device, vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ZMemoryPropertyDeviceFlags);

	GearOutlineInfo infoBig;
	GearOutlineInfo infoSmall;
	uint32_t startIndex = 0;

	// big front
	{
		const std::vector<Vec4> front = generateCogwheelPrimitives(outBig, 0.0f, topo, VK_FRONT_FACE_CLOCKWISE);
		const auto frontWithNormals = resizeWithNormals(front);
		copyByStaging(frontWithNormals, vertices, startIndex, queue);
		infoBig.front = { startIndex, data_count(frontWithNormals) };
		startIndex += infoBig.front.second;
	}
	// big back
	{
		const std::vector<Vec4> back = generateCogwheelPrimitives(outBig, -10.0f, topo, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		const auto backWithNormals = resizeWithNormals(back);
		copyByStaging(backWithNormals, vertices, startIndex, queue);
		infoBig.back = { startIndex, data_count(backWithNormals) };
		startIndex += infoBig.back.second;
	}
	// big surface
	{
		const std::vector<Vec4> surf = generateCogwheelToothSurface(outBig, 0.0f, -10.0f, topo, VK_FRONT_FACE_CLOCKWISE);
		const auto surfWithNormals = resizeWithNormals(surf);
		copyByStaging(surfWithNormals, vertices, startIndex, queue);
		infoBig.surf = { startIndex, data_count(surfWithNormals) };
		startIndex += infoBig.surf.second;
	}

	// small front
	{
		const std::vector<Vec4> front = generateCogwheelPrimitives(outSmall, 0.0f, topo, VK_FRONT_FACE_CLOCKWISE);
		const auto frontWithNormals = resizeWithNormals(front);
		copyByStaging(frontWithNormals, vertices, startIndex, queue);
		infoSmall.front = { startIndex, data_count(frontWithNormals) };
		startIndex += infoSmall.front.second;
	}
	// small back
	{
		const std::vector<Vec4> back = generateCogwheelPrimitives(outSmall, -10.0f, topo, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		const auto backWithNormals = resizeWithNormals(back);
		copyByStaging(backWithNormals, vertices, startIndex, queue);
		infoSmall.back = { startIndex, data_count(backWithNormals) };
		startIndex += infoSmall.back.second;
	}
	// small surface
	{
		const std::vector<Vec4> surf = generateCogwheelToothSurface(outSmall, 0.0f, -10.0f, topo, VK_FRONT_FACE_CLOCKWISE);
		const auto surfWithNormals = resizeWithNormals(surf);
		copyByStaging(surfWithNormals, vertices, startIndex, queue);
		infoSmall.surf = { startIndex, data_count(surfWithNormals) };
		startIndex += infoSmall.surf.second;
	}

	input.binding(0).declareAttributes<Fmt_<Vec4>, Fmt_<Vec3>>();

	ZBuffer indices = createIndexBuffer(input.device, vertexCount, VK_INDEX_TYPE_UINT32);
	BufferTexelAccess<uint32_t> a(indices, vertexCount, 1u, 1u);
	for (uint32_t i = 0u; i < vertexCount; ++i)
		a.at(i, 0) = i;

	return { vertices, indices, infoBig, infoSmall };
}

TriLogicInt runTests(Canvas& cs, add_cref<Params> params)
{
	VertexInput vertexInput(cs.device);
	GearOutlineInfo bigGearInfo, smallGearInfo;
	ZBuffer vertexBuffer, indexBuffer;
	std::tie(vertexBuffer, indexBuffer, bigGearInfo, smallGearInfo) =
		createVertexAndIndexBuffers(vertexInput, params, cs.graphicsQueue);

	ZShaderModule vert, geom, frag;
	std::tie(vert, geom, frag) = buildProgram(cs.device, params);

	const VkFormat		colorFormat = cs.surfaceFormat;
	const VkFormat		depthFormat = formatSelectMaxDepthSupported(cs.device);
	const VkClearValue	clearColor{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };
	const VkClearValue	clearDepth{ { { 1.0f, 0u } } };
	const std::vector<RPA>		colors{ RPA(AttachmentDesc::Presentation, colorFormat, clearColor,
											VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
										RPA(AttachmentDesc::DeptStencil, depthFormat, clearDepth,
											VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) };
	const ZAttachmentPool		attachmentPool(colors);
	const ZSubpassDescription2	subpass({ RPAR(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
											RPAR(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) });
	LayoutManager		lm(cs.device);
	ZBuffer				mvpBuffer = createBuffer<MVP>(cs.device, 1u, ZBufferUsageUniformFlags);
	const uint32_t		mvpBinding = lm.addBinding(mvpBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); UNREF(mvpBinding);
	ZDescriptorSetLayout dsLayout = lm.createDescriptorSetLayout();

	ZPipelineLayout		pipelineLayout = lm.createPipelineLayout({ dsLayout });
	ZRenderPass			renderPass = createRenderPass(cs.device, attachmentPool, subpass);
	ZPipeline			pipeline = createGraphicsPipeline(pipelineLayout, renderPass,
															vert, geom, frag, params.topology, vertexInput.binding(0),
															gpp::SubpassIndex(0),
															gpp::DepthTestEnable(true), gpp::DepthWriteEnable(true),
															VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	const ZQueryPool	queryPool = createQueryPool(cs.device, VK_QUERY_TYPE_PIPELINE_STATISTICS,
													VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
													| VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT);
	ZBuffer				queryResults = createBuffer<uint64_t[2]>(cs.device);

	UserData userData(mvpBuffer);
	cs.events().cbKey.set(onKey, &userData);
	cs.events().cbMouseButton.set(onMouseBtn, &userData);
	cs.events().cbCursorPos.set(onCursorPos, &userData);
	cs.events().cbScroll.set(onScroll, &userData);

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
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);

				vkCmdDrawIndexed(*cmdBuffer, bigGearInfo.front.second, 1,  bigGearInfo.front.first, 0, 0);
				vkCmdDrawIndexed(*cmdBuffer, bigGearInfo.back.second,  1,  bigGearInfo.back.first, 0, 0);
				vkCmdDrawIndexed(*cmdBuffer, bigGearInfo.surf.second,  1,  bigGearInfo.surf.first, 0, 0);

				vkCmdDrawIndexed(*cmdBuffer, smallGearInfo.front.second, 3, smallGearInfo.front.first, 0, 1);
				vkCmdDrawIndexed(*cmdBuffer, smallGearInfo.back.second,  3, smallGearInfo.back.first, 0, 1);
				vkCmdDrawIndexed(*cmdBuffer, smallGearInfo.surf.second,  3, smallGearInfo.surf.first, 0, 1);

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
			updateMatrices(cs, userData);
		}
	};
	FPS stableRedraw(userData.rotateTrigger, stableRedrawCall);

	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		bufferRead(queryResults, userData.queryResult);
		userData.drawTrigger += 1;
		stableRedraw.touch();
		if (params.enableFPS)
		{
			fps.touch();
		}
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

