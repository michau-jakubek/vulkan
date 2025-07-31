#include "descriptorBufferTests.hpp"
#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"
#include "vtfProgressRecorder.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZPipeline.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfMatrix.hpp"

#include <numeric>

namespace
{
using namespace vtf;

struct Params
{
	enum State
	{
		Run,
		Help,
		Error
	};

	add_cref<std::string>	m_assets;
	add_cref<strings>		m_cmdLineParams;
	bool					m_set;
	bool					m_fps;
	bool					m_cache;
	bool					m_samplerAnisotropy;
	Params(add_cref<std::string> assets, add_cref<strings> cmdLineParams)
		: m_assets				(assets)
		, m_cmdLineParams		(cmdLineParams)
		, m_set					(false)
		, m_fps					(false)
		, m_cache				(false)
		, m_samplerAnisotropy	(false)
	{
	}

	State parse ();
	void  print (add_ref<std::ostream> log) const;

private:
	OptionParser<Params> getParser ();
};

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params);

TriLogicInt prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	Params p(record.assets, cmdLineParams);
	if (p.parse() != Params::State::Run)
	{
		return {};
	}

	auto onGetEnabledFeatures = [&](add_ref<DeviceCaps> caps)
	{
		if (false == p.m_set)
		{
			caps.addUpdateFeatureIf(
				&VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress)
				.checkSupported("bufferDeviceAddress");

			caps.addUpdateFeatureIf(
				&VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBuffer)
				.checkSupported("descriptorBuffer");

			//caps.addUpdateFeatureIf(
			//	&VkPhysicalDeviceDescriptorBufferFeaturesEXT::descriptorBufferCaptureReplay)
			//	.checkSupported("descriptorBufferCaptureReplay");

			p.m_samplerAnisotropy = caps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::samplerAnisotropy);

			caps.addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
			caps.addExtension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
			caps.addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
			caps.addExtension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
			caps.addExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		}
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	Canvas	cs(record.name, gf.layers, {}, {}, Canvas::DefaultStyle, onGetEnabledFeatures, gf.apiVer);

	p.print(std::cout);

	return runTests(cs, p);
}

constexpr Option optionSet("-set", 0);
constexpr Option optionFps("-fps", 0);
constexpr Option optionCache("-cache", 0);
OptionParser<Params> Params::getParser ()
{
	OptionFlags				flags	(OptionFlag::None);
	add_ref<Params>			params	= *this;
	OptionParser<Params>	parser	(params);

	parser.addOption(&Params::m_set, optionSet,	"Use descriptor set", { params.m_set }, flags);
	parser.addOption(&Params::m_fps, optionFps, "Enable FPS", { params.m_fps }, flags);
	parser.addOption(&Params::m_cache, optionCache, "Use pipeline cache", { params.m_cache }, flags);

	return parser;
}
Params::State Params::parse ()
{
	OptionParser<Params>	parser	(getParser());
	parser.parse(m_cmdLineParams);
	OptionParserState		state = parser.getState();

	if (state.hasHelp)
	{
		parser.printOptions(std::cout, 40);
		return State::Help;
	}

	if (state.hasErrors || state.hasWarnings)
	{
		std::cout << state.messages.str() << std::endl;
		if (state.hasErrors) return State::Error;
	}

	return State::Run;
}
void Params::print (add_ref<std::ostream> log) const
{
	typedef routine_arg_t<decltype(std::setw), 0> sted_setw_0;
	Params	p		(*this);
	auto	parser	= p.getParser();
	const sted_setw_0 w = static_cast<sted_setw_0>(std::max(24u, parser.getMaxOptionNameLength()));

	for (auto opt : parser.getOptions())
	{
		log << std::left << std::setw(w) << opt->getName() << opt->valueWriter() << std::endl;
	}
}

enum MatriceeUpdateTypes
{
	MATRICES_UPDATE_TYPE_ALL,
	MATRICES_UPDATE_TYPE_ROTATE,
	MATRICES_UPDATE_TYPE_ZOOM_IN,
	MATRICES_UPDATE_TYPE_ZOOM_OUT
};

void updateMatrices (add_ref<Canvas> cs, ZBuffer buffer, MatriceeUpdateTypes mut);

struct UserData {
	ZBuffer matBuffer;
	int drawTrigger;
};

void onScroll (add_ref<Canvas> cs, void_ptr userData, double xScrollOffset, double yScrollOffset)
{
	UNREF(xScrollOffset);
	updateMatrices(cs, reinterpret_cast<add_ptr<UserData>>(userData)->matBuffer,
		(yScrollOffset > 0.0) ? MATRICES_UPDATE_TYPE_ZOOM_IN : MATRICES_UPDATE_TYPE_ZOOM_OUT);
}

void onKey (add_ref<Canvas> cs, void_ptr userData, const int key, int scancode, int action, int mods)
{
	UNREF(userData);	UNREF(scancode);	UNREF(mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
}

struct MVP {
	glm::mat4 model1;
	glm::mat4 model2;
	glm::mat4 view;
	glm::mat4 proj;
	float rotationAngle;
};

void updateMatrices (add_ref<Canvas> cs, ZBuffer buffer, MatriceeUpdateTypes mut)
{
	const float rotateStep = 1.0f / 128.f;
	static float rotateNext = 0.0f;
	rotateNext += rotateStep;
	rotateNext = std::fmod(rotateNext, glm::two_pi<float>());

	const float fowBase = 45.0f;
	const float fowStep = 1.0f;
	static float fowNext = fowBase;

	MVP old;
	if (MATRICES_UPDATE_TYPE_ALL == mut)
	{
		glm::mat4 model = glm::mat4(1.0f);
		//model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		//model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//model = glm::scale(model, glm::vec3(scale, scale, scale));
		const float scale = 0.5f;

		old.model1 = glm::translate(model, glm::vec3(-0.5f, 0.0f, 0.0f));
		old.model1 = glm::rotate(old.model1, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		old.model1 = glm::scale(old.model1, glm::vec3(scale, scale, scale));

		old.model2 = glm::translate(model, glm::vec3(+0.5f, 0.0f, 0.0f));
		old.model2 = glm::rotate(old.model2, glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		old.model2 = glm::scale(old.model2, glm::vec3(scale, scale, scale));

		glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
		glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
		old.view = view;

		const float fov = glm::radians(fowNext);
		const float aspectRatio = float(cs.width) / float(cs.height);
		const float nearPlane = 0.1f;
		const float farPlane = 100.0f;
		glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
		old.proj = projection;
	}
	else
	{
		bufferRead(buffer, old);
	}

	glm::mat4 projection = old.proj;
	if (MATRICES_UPDATE_TYPE_ZOOM_IN == mut || MATRICES_UPDATE_TYPE_ZOOM_OUT == mut)
	{
		if (MATRICES_UPDATE_TYPE_ZOOM_IN == mut)
			fowNext += fowStep;
		else fowNext -= fowStep;
		fowNext = std::clamp(fowNext, 15.0f, 120.0f);

		const float fov = glm::radians(fowNext);
		const float aspectRatio = float(cs.width) / float(cs.height);
		const float nearPlane = 0.1f;
		const float farPlane = 100.0f;
		projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	}

	MVP mvp
	{
		glm::rotate(old.model1, (+rotateStep), glm::vec3(1.f, 1.0f, 0.0f)),
		glm::rotate(old.model2, (-rotateStep), glm::vec3(1.f, 1.0f, 0.0f)),
		old.view,
		projection,
		0.0f
	};

	bufferWrite(buffer, mvp);
}

void makeVertices (add_ref<VertexInput> input)
{
	auto r = [&](const Vec3& p, int face)
	{
		glm::vec3 ax = face < 4 ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		float af = 0.0f;
		switch (face)
		{
		case 4:
			af = 1.0f;
			break;
		case 5:
			af = -1.0f;
			break;
		default:
			af = float(face);
		}
		float ag = 90.0f * af;
		float ar = glm::radians(ag);
		glm::mat4 m = glm::rotate(glm::mat4(1.0f), ar, ax);
		const glm::vec4 rp = m * glm::vec4(glm::vec3(p.x(), p.y(), p.z()), 1.0f);
		return Vec3(rp.x, rp.y, rp.z);
	};

	Vec3 A{ -0.5f, +0.5f, 0.5f };
	Vec3 B{ -0.5f, -0.5f,  0.5f };
	Vec3 C{ +0.5f, -0.5f,  0.5f };
	Vec3 D{ +0.5f, +0.5f,  0.5f };

	const std::vector<Vec3> vertices = {
		A, B, C, C, D, A,	// front
		r(A, 1), r(B, 1), r(C, 1), r(C,1), r(D, 1), r(A, 1),	// right
		r(A, 2), r(B, 2), r(C, 2), r(C,2), r(D, 2), r(A, 2),	// back
		r(A, 3), r(B, 3), r(C, 3), r(C,3), r(D, 3), r(A, 3),	// left
		r(A, 4), r(B, 4), r(C, 4), r(C,4), r(D, 4), r(A, 4),	// top
		r(A, 5), r(B, 5), r(C, 5), r(C,5), r(D, 5), r(A, 5),	// bottom
	};

	float f06 = 0.0f;
	float f16 = 1.0f / 6.0f;
	float f26 = 2.0f / 6.0f;
	float f36 = 3.0f / 6.0f;
	float f46 = 4.0f / 6.0f;
	float f56 = 5.0f / 6.0f;
	float f66 = 1.0f;
	std::vector<Vec2> coords(vertices.size());
	coords = {
		{ f06,0 }, {f06,1}, {f16, 1}, {f16,1}, {f16,0}, {f06,0},
		{ f16,0 }, {f16,1}, {f26, 1}, {f26,1}, {f26,0}, {f16,0},
		{ f26,0 }, {f26,1}, {f36, 1}, {f36,1}, {f36,0}, {f26,0},
		{ f36,0 }, {f36,1}, {f46, 1}, {f46,1}, {f46,0}, {f36,0},
		{ f46,0 }, {f46,1}, {f56, 1}, {f56,1}, {f56,0}, {f46,0},
		{ f56,0 }, {f56,1}, {f66, 1}, {f66,1}, {f66,0}, {f56,0},
	};

	input.binding(0).addAttributes(vertices, coords);
}

TriLogicInt runTests (add_ref<Canvas> canvas, add_cref<Params> params)
{
	add_ref<ProgressRecorder> recorder = canvas.device.getParam<ZPhysicalDevice>()
											.getParam<ZInstance>()
											.getParamRef<ProgressRecorder>();

	const VkFormat stoFormat = VK_FORMAT_R32_UINT;
	const std::string face1FileName = (fs::path(params.m_assets) / "dice_texture.png").string();
	{
		uint32_t	imageWidth = 0;
		uint32_t	imageHeight = 0;
		VkFormat	imageFormat = VK_FORMAT_UNDEFINED;
		ASSERTION(readImageFileMetadata(face1FileName, imageFormat, imageWidth, imageHeight, 4));
		const bool samplingSupported = 0 !=
			(formatGetProperties(deviceGetPhysicalDevice(canvas.device), imageFormat)
				.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
		ASSERTMSG(samplingSupported, formatGetInfo(imageFormat).name, " doesn't support sampling");
		const bool storingSupported = 0 !=
			(formatGetProperties(deviceGetPhysicalDevice(canvas.device), imageFormat)
				.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
		ASSERTMSG(storingSupported, formatGetInfo(imageFormat).name, " doesn't support storing");
	}

	const bool calcFrameRate = params.m_fps;
	const bool useDescriptorSet = params.m_set;
	const VkDescriptorSetLayoutCreateFlags layoutFlags = useDescriptorSet
		? VkDescriptorSetLayoutCreateFlags(0)
		: VkDescriptorSetLayoutCreateFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();

	ZImageUsageFlags	stoUsage	(VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT);
	ZImageUsageFlags	faceUsage	(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_SAMPLED_BIT);
	ZBuffer				face1Data	= createBufferAndLoadFromImageFile(canvas.device,
											face1FileName, ZBufferUsageStorageFlags, 4);
	ZImage				face1Image	= createImageFromFileMetadata(canvas.device, face1Data, faceUsage,
																	(false == useDescriptorSet));
	ZImageView			face1View	= createImageView(face1Image);
	ZSampler			sampler		= createSampler(face1View, true, true, false, params.m_samplerAnisotropy);
	ZImage				stoImage	= createImage(canvas.device, stoFormat, VK_IMAGE_TYPE_2D, 2, 3,
													stoUsage, VK_SAMPLE_COUNT_1_BIT, 1u, 1u, 1u, (false == useDescriptorSet));
	ZImageView			stoView		= createImageView(stoImage);

	VertexInput			vertexInput	(canvas.device);
	makeVertices(vertexInput);

	ProgramCollection	prg			(canvas.device, params.m_assets);
	prg.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT, "shader.comp");
	prg.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	prg.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	prg.buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly);
	ZShaderModule		compShader	= prg.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
	ZShaderModule		vertShader	= prg.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule		fragShader	= prg.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
	struct alignas(16) UniformUint
	{
		uint32_t value;
		UniformUint () : value(0) {}
		UniformUint (uint32_t x) : value(x) {}
		operator uint32_t () const { return value; }
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	LayoutManager			set0		(canvas.device);
	const uint32_t			elemCount	= 64;
	const uint32_t			startElem1	= 7;
	const uint32_t			startElem2	= 19;
	const uint32_t			inBinding1	= set0.addBindingAsVector<UniformUint>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, elemCount);
	const uint32_t			imgBinding	= set0.addBinding(face1View, ZSampler());
	const uint32_t			outBinding1	= set0.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, elemCount);
	const uint32_t			compBinding = set0.addBinding(face1View, sampler);
	const uint32_t			outBinding2	= set0.addBindingAsVector<uint32_t>(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, elemCount);
	const uint32_t			samBinding	= set0.addBinding(ZImageView(), sampler);
	const uint32_t			inBinding2	= set0.addBindingAsVector<UniformUint>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, elemCount);
	const uint32_t			stoBinding	= set0.addBinding(stoView, ZSampler(), VK_IMAGE_LAYOUT_GENERAL,
															VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	ZDescriptorSetLayout	ds0Layout	= set0.createDescriptorSetLayout(useDescriptorSet, layoutFlags);

	LayoutManager			set1		(canvas.device);
	const uint32_t			stoBinding2	= set1.addBinding(stoView, ZSampler(), VK_IMAGE_LAYOUT_GENERAL,
															VK_DESCRIPTOR_TYPE_STORAGE_IMAGE); UNREF(stoBinding2);
	const uint32_t			matBinding	= set1.addBinding<MVP>(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	ZDescriptorSetLayout	ds1Layout	= set1.createDescriptorSetLayout(useDescriptorSet, layoutFlags);

	ZPipelineLayout			pLayout		= set0.createPipelineLayout({ds0Layout, ds1Layout});
	ZPipeline				compPline	= createComputePipeline(pLayout, compShader);
	ZBuffer					desc0Buffer	= useDescriptorSet ? ZBuffer() : set0.createDescriptorBuffer(ds0Layout);
	ZBuffer					desc1Buffer = useDescriptorSet ? ZBuffer() : set1.createDescriptorBuffer(ds1Layout);
	ZRenderPass				renderPass	= createColorRenderPass(canvas.device, { canvas.surfaceFormat },
																{ makeClearColor(Vec4(0,0,0,1)) });
	ZPipelineCache			graphCache = params.m_cache
											? createPipelineCache(canvas.device, "descriptor_buffer.cache")
											: ZPipelineCache();
	recorder.stamp("Before vkCreateGraphicsPipelines()");
	ZPipeline				graphPline	= createGraphicsPipeline(pLayout, renderPass, vertShader, fragShader, vertexInput,
																VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
																gpp::DepthTestEnable(true), graphCache);
	recorder.stamp("After vkCreateGraphicsPipelines()");
	ZBuffer					matBuffer	= std::get<DescriptorBufferInfo>(set1.getDescriptorInfo(matBinding)).buffer;
	ZBuffer					inStoBuffer	= createBuffer(stoImage, ZBufferUsageStorageFlags, ZMemoryPropertyHostFlags);
	ZBuffer					outStoBuffer = createBuffer(stoImage, ZBufferUsageStorageFlags, ZMemoryPropertyHostFlags);
	ZBuffer					outBuffer1	= std::get<DescriptorBufferInfo>(set0.getDescriptorInfo(outBinding1)).buffer;
	ZBuffer					outBuffer2	= std::get<DescriptorBufferInfo>(set0.getDescriptorInfo(outBinding2)).buffer;
	ZBuffer					inBuffer1	= std::get<DescriptorBufferInfo>(set0.getDescriptorInfo(inBinding1)).buffer;
	ZBuffer					inBuffer2	= std::get<DescriptorBufferInfo>(set0.getDescriptorInfo(inBinding2)).buffer;
	{
		std::vector<UniformUint> inData(bufferGetElementCount<UniformUint>(inBuffer1));
		std::iota(inData.begin(), inData.end(), startElem1);
		bufferWrite(inBuffer1, inData);
		std::iota(inData.begin(), inData.end(), startElem2);
		bufferWrite(inBuffer2, inData);
		std::vector<uint32_t> stoData(bufferGetElementCount<uint32_t>(inStoBuffer));
		std::fill(stoData.begin(), stoData.end(), (startElem1 + startElem2));
		bufferWrite(inStoBuffer, stoData);
	}
	{
		OneShotCommandBuffer shot(canvas.device, canvas.graphicsQueue);
		bufferCopyToImage(shot.commandBuffer, face1Data, face1Image,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		bufferCopyToImage(shot.commandBuffer, inStoBuffer, stoImage,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}

	MULTI_UNREF(inBinding1, inBinding2, outBinding1, outBinding2, samBinding, imgBinding, stoBinding, compBinding);

	if (true)
	{
		std::cout << "SampledImage: " << face1Image << std::endl
				  << "StorageImage: " << stoImage << std::endl;
	}
	if (params.m_cache)
	{
		recorder.print(std::cout);
	}

	UserData userData{ matBuffer, 1u };
	bool swapchainRecretaed = false;
	canvas.events().cbKey.set(onKey, &userData);
	canvas.events().cbScroll.set(onScroll, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain, ZCommandBuffer cmd, ZFramebuffer framebuffer)
	{
		swapchainRecretaed = swapchain.recreateFlag;
		commandBufferBegin(cmd);
		if (useDescriptorSet)
			commandBufferBindPipeline(cmd, compPline, useDescriptorSet);
		else commandBufferBindDescriptorBuffers(cmd, compPline, { desc0Buffer, desc1Buffer });
		commandBufferDispatch(cmd);
		imageCopyToBuffer(cmd, stoImage, outStoBuffer,
			VK_ACCESS_SHADER_WRITE_BIT, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		if (useDescriptorSet)
			commandBufferBindPipeline(cmd, graphPline, useDescriptorSet);
		else commandBufferBindDescriptorBuffers(cmd, graphPline, { desc0Buffer, desc1Buffer });
		commandBufferBindVertexBuffers(cmd, vertexInput);
		vkCmdSetViewport(*cmd, 0, 1, &swapchain.viewport);
		vkCmdSetScissor(*cmd, 0, 1, &swapchain.scissor);
		auto rpbi = commandBufferBeginRenderPass(cmd, framebuffer, 0);
			vkCmdDraw(*cmd, vertexInput.getVertexCount(0), 2, 0, 0);
		commandBufferEndRenderPass(rpbi);
		imageCopyToBuffer(cmd, stoImage, outStoBuffer,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
			VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		commandBufferMakeImagePresentationReady(cmd, framebufferGetImage(framebuffer));
		commandBufferEnd(cmd);
	};

	FPS fps([caretBack = std::string(16, '\x8')](float fps, float totalTime, float bestFPS, float worstFPS)
	{
		UNREF(totalTime), UNREF(bestFPS), UNREF(worstFPS);
		std::cout << caretBack << "FPS: " << fps;
	});
	bool compareOnce = true;
	int compareBuffersResult = 0;
	auto onAfterRecording = [&](add_ref<Canvas>)
	{
		updateMatrices(canvas, matBuffer, (swapchainRecretaed ? MATRICES_UPDATE_TYPE_ALL : MATRICES_UPDATE_TYPE_ROTATE));
		userData.drawTrigger += 1;

		if (calcFrameRate)
		{			
			fps.touch();
		}

		if (compareOnce)
		{
			compareOnce = false;

			std::vector<UniformUint> inData(bufferGetElementCount<UniformUint>(inBuffer1));
			bufferRead(inBuffer1, inData);
			std::vector<uint32_t> outData(bufferGetElementCount<uint32_t>(outBuffer1));
			bufferRead(outBuffer1, outData);

			if (inData.size() == outData.size() && inData.size() == elemCount)
			{
				uint32_t elemValue = startElem1;
				for (uint32_t k = 0u; k < elemCount; ++k, ++elemValue)
				{
					if (elemValue != inData[k] || elemValue != outData[k])
					{
						std::cout << "Buffer1 mismatch at index: " << k << ", expected " << elemValue
							<< ", got " << inData[k] << " & " << outData[k] << std::endl;
						compareBuffersResult += 1;
						break;
					}
				}

				elemValue = startElem2;
				bufferRead(inBuffer2, inData);
				bufferRead(outBuffer2, outData);
				for (uint32_t k = 0u; k < elemCount; ++k, ++elemValue)
				{
					if (elemValue != inData[k] || elemValue != outData[k])
					{
						std::cout << "Buffer2 mismatch at index: " << k << ", expected " << elemValue
							<< ", got " << inData[k] << " & " << outData[k] << std::endl;
						compareBuffersResult += 1;
						break;
					}
				}
			}
			else
			{
				std::cout << "Mismatch parameters" << std::endl;
				compareBuffersResult = 2;
			}

			const uint32_t stoCount = bufferGetElementCount<uint32_t>(outStoBuffer);
			std::vector<uint32_t> stoData(stoCount);
			bufferRead(outStoBuffer, stoData);
			if (startElem1 != stoData[0] || startElem2 != stoData[1]
				|| (startElem1 + 1) != stoData[2] || (startElem2 + 1) != stoData[3]
				|| (startElem1 + 2) != stoData[4] || (startElem2 + 2) != stoData[5])
			{
				std::cout << "Storage image content mismatch, "
					<< "expected: " << VecX<uint32_t, 6>(startElem1, startElem2,
														startElem1+1, startElem2+1,
														startElem1+2, startElem2+2)
					<< ", got: ";
				for (uint32_t i = 0; i < stoCount; ++i)
				{
					if (i) std::cout << ", ";
					std::cout << stoData[i];
				}
				std::cout << std::endl;
				compareBuffersResult += 1;
			}
		}
	};

	const int runResult = canvas.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger), {}, onAfterRecording);

	return (runResult + compareBuffersResult);
}

} // unnamed namespace

template<> struct TestRecorder<DESCRIPTOR_BUFFER>
{
	static bool record(TestRecord&);
};
bool TestRecorder<DESCRIPTOR_BUFFER>::record(TestRecord& record)
{
	record.name = "descriptor_buffer";
	record.call = &prepareTests;
	return true;
}
