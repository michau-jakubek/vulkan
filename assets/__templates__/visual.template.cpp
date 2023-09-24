#include "allTests.hpp"	// Option

//#include "triangleTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfZImage.hpp"
#include "vtfPipelineLayout.hpp"
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

struct Params
{
	uint32_t param1;
};

Params readCommandLine (add_cref<strings> cmdLineParams, add_cref<Params> defaultParams, add_ref<std::ostream> log)
{
	bool				status = false;
	strings				sink;
	strings				args(cmdLineParams);
	Option				opt1 { "-param1", 1 };
	std::vector<Option>	options { opt1 };
	Params				resultParams{};
	if (consumeOptions(opt1, options, args, sink) > 0)
	{
		resultParams.param1 = fromText(sink.back(), defaultParams.param1, status);
		if (!status)
		{
			log << "[WARNING] Unable to parse param1.\n"
				   "          Assuming default value of " << defaultParams.param1 << std::endl;
		}
	}
	return resultParams;
}

Params printUsage (std::ostream& log)
{
	Params defaultParams{};
	defaultParams.param1 = 17;

	log << "Parameters:\n"
		<< "  [-param1 <uint:num>] param1, default is " << defaultParams.param1 << '\n'
		<< "  [-i <uint:index_type>] type of index used by shader\n"
		<< "Navigation keys:\n"
		<< "  Escape: quit this app"
		<< std::endl;
	return defaultParams;
}

int runTemplateTemplateSingleThread (add_ref<Canvas> canvas, add_cref<std::string> assets, add_cref<Params> params);

UNUSED int prepareTests (const TestRecord& record, const strings& cmdLineParams)
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

	VkPhysicalDeviceFeatures2 someFeaturesToVerify = makeVkStruct();
	const std::string someExtensionToVerify("SOME_EXTENSION_TO_VERIFY");
	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		VkPhysicalDeviceFeatures2 resultFeatures = makeVkStruct();

		if (containsString(someExtensionToVerify, extensions))
		{
			resultFeatures.pNext = &someFeaturesToVerify;
			vkGetPhysicalDeviceFeatures2(*physicalDevice, &resultFeatures);
		}

		return resultFeatures;
	};
	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, onEnablingFeatures, gf.debugPrintfEnabled, gf.apiVer);
	if (!someFeaturesToVerify.features.fragmentStoresAndAtomics)
	{
		std::cout << "[ERROR] " << someExtensionToVerify << " not supported by device" << std::endl;
		return 1;
	}

	std::ostream& log = std::cout;	
	const Params defaultParams = printUsage(log);
	return runTemplateTemplateSingleThread (cs, record.assets, readCommandLine(cmdLineParams, defaultParams, log));
}

struct UserData
{
	int		drawTrigger;
	double	windowXXXcursor;
	double	windowYYYcursor;
	double	miceButtonXXXcursor;
	double	miceButtonYYYcursor;
	bool	miceButtonPressed;
	float	zoom;
	UserData ();
};
UserData::UserData ()
	: drawTrigger			(1)
	, windowXXXcursor		(0.0)
	, windowYYYcursor		(0.0)
	, miceButtonXXXcursor	(0.0)
	, miceButtonYYYcursor	(0.0)
	, miceButtonPressed		(false)
	, zoom					(1.0f)
{
}

void onResize (add_ref<Canvas>, add_ptr<void> userData, int width, int height)
{
	UNREF(width);
	UNREF(height);
	if (userData)
	{
		static_cast<add_ptr<UserData>>(userData)->drawTrigger += 1;
	}
}

void onKey (add_ref<Canvas> cs, add_ptr<void> userData, const int key, int scancode, int action, int mods)
{
	UNREF(userData);
	UNREF(scancode);
	UNREF(mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
}

void onMouseBtn (add_ref<Canvas>, add_ptr<void> userData, int button, int action, int mods)
{
	UNREF(button);
	UNREF(action);
	UNREF(mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		auto ui = static_cast<add_ptr<UserData>>(userData);
		ui->miceButtonXXXcursor = ui->windowXXXcursor;
		ui->miceButtonYYYcursor = ui->windowYYYcursor;
		ui->miceButtonPressed = (action == GLFW_PRESS);
	}
}

void onCursorPos (add_ref<Canvas> cs, add_ptr<void> userData, double xpos, double ypos)
{
	auto ui = static_cast<add_ptr<UserData>>(userData);
	ui->windowXXXcursor = xpos;
	ui->windowYYYcursor = ypos;

	if (ui->miceButtonPressed)
	{
		++ui->drawTrigger;
		const double wx = xpos - ui->miceButtonXXXcursor;
		const double wy = ypos - ui->miceButtonYYYcursor;
		const float dx = float(wx) / float(cs.width);	UNREF(dx);
		const float dy = float(wy) / float(cs.height);	UNREF(dy);
		// here, you can do something with dx and dy
	}
}

void onScroll (add_ref<Canvas>, add_ptr<void> userData, double xScrollOffset, double yScrollOffset)
{
	UNREF(xScrollOffset);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	++ui->drawTrigger;

	if (yScrollOffset > 0.0)
		ui->zoom *= 1.125f;
	else
		ui->zoom /= 1.125f;
}

std::array<ZShaderModule, 5> buildProgram (ZDevice device, add_cref<std::string> assets)
{
	ProgramCollection			programs(device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "shader.frag");
	const GlobalAppFlags		flags(getGlobalAppFlags());
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate);

	std::array<ZShaderModule, 5> shaders
	{
		*programs.getShader(VK_SHADER_STAGE_VERTEX_BIT),
		*programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	return shaders;
}

void populateVertexAttributes (add_ref<VertexInput> vertexInput)
{
	std::vector<Vec2>		vertices	{ { -1, +1 }, { 0, -1 }, { +1, +1 } };
	std::vector<Vec3>		colors		{ { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	vertexInput.binding(0).addAttributes(vertices, colors);
}

int runTemplateTemplateSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets, add_cref<Params> params)
{
	UNREF(params);
	PipelineLayout				pl(cs.device);

	std::array<ZShaderModule,5> shaders			= buildProgram(cs.device, assets);
	ZShaderModule				vertShaderModule= shaders[0];
	ZShaderModule				fragShaderModule= shaders[1];

	VertexInput					vertexInput			(cs.device);
	populateVertexAttributes(vertexInput);

	const VkFormat				format				= cs.surfaceFormat;
	const VkClearValue			clearColor			{ { { 0.5f, 0.5f, 0.5f, 0.5f } } };

	ZRenderPass					renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});

	UserData userData;
	cs.events().cbKey.set(onKey, &userData);
	cs.events().cbScroll.set(onScroll, &userData);
	cs.events().cbWindowSize.set(onResize, &userData);
	cs.events().cbCursorPos.set(onCursorPos, &userData);
	cs.events().cbMouseButton.set(onMouseBtn, &userData);

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain>, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		ZImage				renderImage			= framebufferGetImage(framebuffer);

		commandBufferBegin(cmdBuffer);
			commandBufferMakeImagePresentationReady(cmdBuffer, renderImage);
		commandBufferEnd(cmdBuffer);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger));
}

} // unnamed namespace

/*
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
*/
