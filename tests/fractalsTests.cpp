
#include "allTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZPipeline.hpp"
#include "vtfVector.hpp"

#include <array>
#include <variant>

using namespace vtf;

namespace
{

#define METH0 "0: z_0 = seed; z_n+1 = z_n^2 + point"
#define METH1 "1: z_0 = point; z_n+1 = z_n^2 + seed"

constexpr int32_t minIters = 50;
struct TestConfig
{
	Vec2		seed;
	uint32_t	ticks;
	int32_t		iters;
	bool		method;
	bool		float32;
			TestConfig	(add_cref<strings>		commandLineParams);
	void	print		(add_ref<std::ostream>	str) const;
};
void TestConfig::print (add_ref<std::ostream> str) const
{
	str << "Animation:                ";
	if (ticks)
		str << "every " << ticks << " milliseconds";
	else str << "disabled";
	str << std::endl;

	str << "Floating point in shader: Float" << (float32 ? "32" : "64") << std::endl;
	str << "Starting point or seed:   " << seed << std::endl;
	str << "Calculation method:       " << (method ? METH1 : METH0) << std::endl;
	str << "Iteration count:          " << iters << std::endl;
}
TestConfig::TestConfig (add_cref<strings> commandLineParams)
	: seed		()
	, ticks		()
	, iters		(minIters)
	, method	()
	, float32	()
{
	strings				sink;
	bool				status				(false);
	strings				args				(commandLineParams);
	Option				optMethod			{ "-m", 1 };
	Option				optStartingPoint	{ "-s", 1 };
	Option				optAnimationTicks	{ "-a", 1 };
	Option				optIterationCount	{ "-i", 1 };
	Option				optEnforceFloat32	{ "-float32", 0 };
	std::vector<Option>	options{ optMethod, optStartingPoint, optAnimationTicks, optIterationCount, optEnforceFloat32 };
	if (consumeOptions(optMethod, options, args, sink) > 0)
	{
		method = fromText(sink.back(), 0u, status) != 0u;
	}
	if (consumeOptions(optStartingPoint, options, args, sink) > 0)
	{
		std::array<bool, 2> statuses;
		seed = Vec2::fromText(sink.back(), seed, statuses);
	}
	if (consumeOptions(optAnimationTicks, options, args, sink) > 0)
	{
		ticks = fromText(sink.back(), 0u, status);
	}
	if (consumeOptions(optIterationCount, options, args, sink) > 0)
	{
		const int32_t i = fromText(sink.back(), 0, status);
		iters = std::abs(i) + minIters;
	}
	float32 = (consumeOptions(optEnforceFloat32, options, args, sink) > 0);
}

TriLogicInt performTest (add_ref<Canvas> canvas, add_cref<std::string> assets,
						 add_cref<TestConfig> config, add_cref<GlobalAppFlags> flags);

TriLogicInt prepareTest (add_cref<TestRecord> record, add_cref<strings> commandLineParams)
{
	std::cout <<
		"Parameters:\n"
		"  [-float32]         Enforce using Float32 in shader\n"
		"                     Otherwise Float64 will be used if available\n"
		"  [-a <msecs>]       Run in animation mode, refresh every milliseconds\n"
		"                      Hold pressed left mouse button or Ctrl key to zoom in\n"
		"                      Hold pressed right mouse button or Ctrl key to zoom out\n"
		"  [-s <float,float>] Set starting point or seed, default is \'0,0\'\n"
		"  [-i <uint]         Set iteration count at start or reset, always " << minIters << "+\n"
		"  [-m <bool>]        Set method used to generating a serie\n"
		"                      " METH0 "\n"
		"                      " METH1 "\n"
		"\n"
		"Navigation keys combination:\n"
		"  Scroll Left|Right|Up|Down: Move the picture slowly\n"
		"  Ctrl or|and Mice Button pressed:\n"
		"   * cursor move:            Move the picture fast\n"
		"   * scroll dowm:            Zoom in\n"
		"   * scroll dowm:            Zoom out\n"
		"   (Mice's buttons might not work the same way on some platforms)\n"
		"  R:                         Restore to start settings\n"
		"  S:                         Print diagnostic information\n"
		"  Gray(+)                    Increment iteration count\n"
		"  Gray(-)                    Decrement iteration count\n"
		"  Esc:                       Quit Fractals\n\n";

	TestConfig	config(commandLineParams);

	bool		enableFloat64 = false;
	auto onGetEnabledFeatures = [&](ZPhysicalDevice physDevice, add_ref<strings> /*deviceExtensions*/) -> VkPhysicalDeviceFeatures2
	{
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(*physDevice, &features);

		VkPhysicalDeviceFeatures2 result{};
		if (!config.float32)
		{
			enableFloat64 = (features.shaderFloat64 != VK_FALSE);
			result.features.shaderFloat64 = features.shaderFloat64;
		}
		return result;
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	Canvas	cs(record.name, gf.layers, {}, {}, Canvas::DefaultStyle, onGetEnabledFeatures, gf.apiVer);

	config.float32 = !enableFloat64;
	config.print(std::cout);

	return performTest(cs, record.assets, config, gf);
}

template<class Float>
struct PushConstant
{
	Float		width;
	Float		height;
	Float		xMin;
	Float		xMax;
	Float		yMin;
	Float		yMax;
	Float		xSeed;
	Float		ySeed;
	int			iterCount;
	uint32_t	mode;
};
typedef Canvas::Swapchain Swapchain;
struct UserInput_
{
	int					drawTrigger;
	double				xCursor;
	double				yCursor;
	bool				ctrlPressed;
	bool				leftCtrlPressed;
	bool				rightCtrlPressed;
	bool				micePressed;
	bool				leftMicePressed;
	bool				rightMicePressed;
	double				miceKeyXcursor;
	double				miceKeyYcursor;
	mutable int			iterationCount;
	const int			iterationStep;
	const bool			adaptiveIteration;
	int					iterationTick;
	bool				animationEnabled;
	uint64_t			animationTicks;
	UserInput_ ()
		: drawTrigger		(1)
		, xCursor			()
		, yCursor			()
		, ctrlPressed		()
		, leftCtrlPressed	()
		, rightCtrlPressed	()
		, micePressed		()
		, leftMicePressed	()
		, rightMicePressed	()
		, miceKeyXcursor	()
		, miceKeyYcursor	()
		, iterationCount	(minIters)
		, iterationStep		(20)
		, adaptiveIteration	(true)
		, iterationTick		(1)
		, animationEnabled	(false)
		, animationTicks	(0)
	{
	}
	virtual void reset (add_cref<Swapchain>) = 0;
	virtual void updateDim (add_cref<Swapchain>) = 0;
	virtual void setup (add_cref<Swapchain>, add_cref<TestConfig>) = 0;
};

template<class Float>
struct UserInput : UserInput_
{
	PushConstant<Float>	pc;
	UserInput() : pc() {}
	virtual void setup (add_cref<Swapchain> swapchain, add_cref<TestConfig> config) override
	{
		reset(swapchain);
		pc.xSeed = config.seed.x();
		pc.ySeed = config.seed.y();
		pc.mode = config.method;
		pc.iterCount = config.iters;
		iterationCount = config.iters;
	}
	virtual void reset (add_cref<Swapchain> swapchain) override
	{
		pc.iterCount = iterationCount;
		updateDim(swapchain);
		pc.xMin = -2.5f;
		pc.xMax = +1.5f;
		pc.yMin = -2.0f;
		pc.yMax = +2.0f;
		ctrlPressed = leftCtrlPressed = rightCtrlPressed = false;
		micePressed	= leftMicePressed = rightMicePressed = false;
		glfwGetCursorPos(*swapchain.canvas.window, &xCursor, &yCursor);
	}
	virtual void updateDim (add_cref<Canvas::Swapchain> swapchain) override
	{
		pc.width	= Float(swapchain.extent.width);
		pc.height	= Float(swapchain.extent.height);
	}
	void incIterationCount (int step = 0, uint32_t tick = 1)
	{
		if (iterationTick + 1 == std::numeric_limits<int>::max())
			iterationTick = 0;
		else iterationTick = iterationTick + 1;

		step = !step ? iterationStep : std::abs(step);

		const bool inc = tick ? (((uint32_t)iterationTick % tick) == 0) : true;

		if (inc) pc.iterCount += step;
	}
	void decIterationCount (int step = 0, uint32_t tick = 1)
	{
		if (iterationTick + 1 == std::numeric_limits<int>::max())
			iterationTick = 0;
		else iterationTick = iterationTick + 1;

		step = !step ? iterationStep : std::abs(step);

		const bool inc = tick ? (((uint32_t)iterationTick % tick) == 0) : true;

		if (inc && pc.iterCount > iterationCount)
			pc.iterCount = std::max(iterationCount, (pc.iterCount - step));
	}
};
typedef std::variant<UserInput<float>, UserInput<double>>	VarUserInput;
VarUserInput selectUserInputVariant (bool enableFloat64)
{
	VarUserInput vui;
	if (enableFloat64)
		vui.emplace<UserInput<double>>();
	else vui.emplace<UserInput<float>>();
	return vui;
}

template<class Float>
Canvas::Area<Float> areaFromPC (const PushConstant<Float>& pc)
{
	Canvas::Area<Float> a;
	a.xMin = pc.xMin;
	a.xMax = pc.xMax;
	a.yMin = pc.yMin;
	a.yMax = pc.yMax;
	return a;
}

template<class Float>
void areaToPC (const Canvas::Area<Float>& a, PushConstant<Float>& pc)
{
	pc.xMin = a.xMin;
	pc.xMax = a.xMax;
	pc.yMin = a.yMin;
	pc.yMax = a.yMax;
}

template<class Float>
void onResize (Canvas& canvas, void* userData, int width, int height)
{
	UNREF(canvas);
	UNREF(width);
	UNREF(height);
	UserInput<Float>* ui = reinterpret_cast<UserInput<Float>*>(userData);
	++ui->drawTrigger;
}

template<class Float>
void onKey (Canvas& cs, void* userData, int key, int scancode, int action, int mods)
{
	UNREF(scancode);
	UNREF(mods);
	UserInput<Float>* ui = reinterpret_cast<UserInput<Float>*>(userData);
	++ui->drawTrigger;
	if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
	{
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->ctrlPressed = (GLFW_PRESS == action || GLFW_REPEAT == action);
		if (key == GLFW_KEY_LEFT_CONTROL)
			ui->leftCtrlPressed = ui->ctrlPressed;
		else ui->rightCtrlPressed = ui->ctrlPressed;
	}
	else if (action == GLFW_PRESS)
	{
		switch (key)
		{
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
				break;

			case GLFW_KEY_R:
				ui->reset(cs);
				break;

			case GLFW_KEY_S:
				std::cout << "xMin:" << ui->pc.xMin << ", xMax:" << ui->pc.xMax << std::endl;
				std::cout << "yMin:" << ui->pc.yMin << ", yMax:" << ui->pc.yMax << std::endl;
				std::cout << "iter:" << ui->pc.iterCount << std::endl;
				if (ui->animationEnabled) {
					std::cout << "Anime ticks: " << ui->animationTicks << std::endl;
					if (ui->animationTicks) {
						std::cout << "Pseudo FPS: " << (1.0e3 / double(ui->animationTicks)) << std::endl;
					}
				}
				break;

			case GLFW_KEY_KP_ADD:
				ui->incIterationCount();
				break;

			case GLFW_KEY_KP_SUBTRACT:
				ui->decIterationCount();
				break;
		}
	}
}

template<class Float>
void onMouseBtn (Canvas&, void* userData, int button, int action, int mods)
{
	UNREF(mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		UserInput<Float>* ui = reinterpret_cast<UserInput<Float>*>(userData);
		++ui->drawTrigger;
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->micePressed = (action == GLFW_PRESS);
		if (button == GLFW_MOUSE_BUTTON_LEFT)
			ui->leftMicePressed = ui->micePressed;
		else ui->rightMicePressed = ui->micePressed;
	}
}

template<class Float>
void onCursorPos (Canvas& canvas, void* userData, double xpos, double ypos)
{
	UNREF(canvas);

	UserInput<Float>* ui = reinterpret_cast<UserInput<Float>*>(userData);
	++ui->drawTrigger;

	ui->xCursor = xpos;
	ui->yCursor = ypos;

	if (ui->micePressed || ui->ctrlPressed)
	{
		double wx = ui->miceKeyXcursor - xpos;
		double wy = ypos - ui->miceKeyYcursor;
		Canvas::Area a = areaFromPC(ui->pc);
		Float dx = (Float(wx) * a.width()) / Float(canvas.width);
		Float dy = (Float(wy) * a.height()) / Float(canvas.height);
		a.move(dx, dy);
		areaToPC(a, ui->pc);
		ui->miceKeyXcursor = xpos;
		ui->miceKeyYcursor = ypos;
	}
}

template<class Float>
void onScroll (Canvas& canvas, void* userData, double xScrollOffset, double yScrollOffset)
{
	auto allowScroll = [&](UserInput<Float>* ui) {
		return	(yScrollOffset != 0.0)
				&& (ui->xCursor >= 2.0 && ui->xCursor < double(canvas.width - 1))
				&& (ui->yCursor >= 2.0 && ui->yCursor < double(canvas.height - 1));
	};

	// xScrollOffset > 0 - scroll right
	// yScrollOffset > 0 - scroll down

	const Float width = Float(canvas.width);	UNREF(width);
	const Float height = Float(canvas.height);	UNREF(height);
	UserInput<Float>* ui = reinterpret_cast<UserInput<Float>*>(userData);
	++ui->drawTrigger;

	if (ui->ctrlPressed || ui->micePressed)
	{
		if (!allowScroll(ui)) return;

		Float scale = Float(1.0) + Float(1.0) / Float(32.0);
		if (yScrollOffset > 0.0)
		{
			scale = Float(1.0) / scale;

			if (ui->adaptiveIteration)
				ui->incIterationCount(8,10);
		}
		else if (ui->adaptiveIteration)
			ui->decIterationCount(8,10);

		VecX<Float,2> oldPoint;
		Canvas::Area<Float> a = areaFromPC(ui->pc);
		canvas.windowToUser(a, VecX<Float,2>(ui->xCursor, (height - ui->yCursor)), oldPoint);

		a.scale(scale);
		VecX<Float,2> newPoint = oldPoint * scale;
		Float xoffset = oldPoint.x() - newPoint.x();
		Float yoffset = oldPoint.y() - newPoint.y();
		a.move(xoffset, yoffset);
		areaToPC(a, ui->pc);
	}
	else
	{
		const Canvas::Area<Float> a = areaFromPC(ui->pc);
		const Float dx = (a.width() * Float(3.0)) / width;
		const Float dy = (a.height() * Float(2.0f)) / height;
		ui->pc.xMin -= static_cast<Float>(dx * xScrollOffset);
		ui->pc.xMax -= static_cast<Float>(dx * xScrollOffset);
		ui->pc.yMin += static_cast<Float>(dy * yScrollOffset);
		ui->pc.yMax += static_cast<Float>(dy * yScrollOffset);
	}
}

void initEvents (Canvas& cs, VarUserInput& vui, bool forceFloat32)
{
	if (forceFloat32)
	{
		cs.events().cbWindowSize.set(onResize<float>,		&std::get<UserInput<float>>(vui));
		cs.events().cbCursorPos.set(onCursorPos<float>,		&std::get<UserInput<float>>(vui));
		cs.events().cbScroll.set(onScroll<float>,			&std::get<UserInput<float>>(vui));
		cs.events().cbMouseButton.set(onMouseBtn<float>,	&std::get<UserInput<float>>(vui));
		cs.events().cbKey.set(onKey<float>,					&std::get<UserInput<float>>(vui));
	}
	else
	{
		cs.events().cbWindowSize.set(onResize<double>,		&std::get<UserInput<double>>(vui));
		cs.events().cbCursorPos.set(onCursorPos<double>,	&std::get<UserInput<double>>(vui));
		cs.events().cbScroll.set(onScroll<double>,			&std::get<UserInput<double>>(vui));
		cs.events().cbMouseButton.set(onMouseBtn<double>,	&std::get<UserInput<double>>(vui));
		cs.events().cbKey.set(onKey<double>,				&std::get<UserInput<double>>(vui));
	}
}

using namespace std::placeholders;
UNUSED void commandBufferPushConstants (ZCommandBuffer cmdBuffer, ZPipelineLayout pipelineLayout,
										const VarUserInput& vui, add_cref<TestConfig> config)
{
	if (config.float32)
		::vtf::commandBufferPushConstants(cmdBuffer, pipelineLayout, std::get<UserInput<float>>(vui).pc);
	else ::vtf::commandBufferPushConstants(cmdBuffer, pipelineLayout, std::get<UserInput<double>>(vui).pc);
}

TriLogicInt performTest (add_ref<Canvas> cs, add_cref<std::string> assets,
						 add_cref<TestConfig> config, add_cref<GlobalAppFlags> flags)
{
	ProgramCollection		programs(cs.device, assets);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, "shader.vert");
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, (config.float32 ? "fshader.frag" : "dshader.frag"));
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);

	ZShaderModule			vertShaderModule	= programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule			fragShaderModule	= programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);
	VertexInput				vertexInput			(cs.device);

	// Scope destruction of a vector
	{
		std::vector<Vec2>	vertices;

		vertices.emplace_back(-1, -1);
		vertices.emplace_back(+1, -1);
		vertices.emplace_back(-1, +1);
		vertices.emplace_back(-1, +1);
		vertices.emplace_back(+1, -1);
		vertices.emplace_back(+1, +1);

		vertexInput.binding(0).addAttributes(vertices);
	}

	LayoutManager			pm					(cs.device);
	const VkClearValue		clearColor			= {{{0.5f, 0.5f, 0.5, 0.5f}}};
	const VkFormat			format				= cs.surfaceFormat;
	ZRenderPass				renderPass			= createColorRenderPass(cs.device, {format}, {{clearColor}});
	ZPipelineLayout			pipelineLayout		= config.float32
													? pm.createPipelineLayout<PushConstant<float>>()
													: pm.createPipelineLayout<PushConstant<double>>();
	ZPipeline				pipeline			= createGraphicsPipeline(pipelineLayout, renderPass,
																		 vertexInput, vertShaderModule, fragShaderModule,
																		 VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	VarUserInput			vui					= selectUserInputVariant(!config.float32);
	add_ref<UserInput_>		ui					= config.float32
													? static_cast<add_ref<UserInput_>>(std::get<UserInput<float>>(vui))
													: static_cast<add_ref<UserInput_>>(std::get<UserInput<double>>(vui));
	ui.setup(cs, config);
	ui.animationEnabled = config.ticks != 0;

	initEvents(cs, vui, (config.float32));

	std::chrono::time_point<std::chrono::steady_clock>
			start = std::chrono::steady_clock::now(); // in nanoseconds

	auto onIdle = [&](add_ref<Canvas> canvas, add_ref<int>)
	{
		const auto now = std::chrono::steady_clock::now();
		const auto tickCount = make_unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());

		if (tickCount > config.ticks)
		{
			start = now;
			ui.animationTicks = tickCount;
			if (ui.ctrlPressed || ui.micePressed)
			{
				if (config.float32)
					onScroll<float>(canvas, &ui, 0.0, ((ui.leftCtrlPressed || ui.leftMicePressed) ? +1.0 : -1.0));
				else
					onScroll<double>(canvas, &ui, 0.0, ((ui.leftCtrlPressed || ui.leftMicePressed) ? +1.0 : -1.0));
			}
		}
	};

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		ui.updateDim(swapchain);

		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, pipeline);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			commandBufferPushConstants(cmdBuffer, pipelineLayout, vui, config);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &swapchain.scissor);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer, 0);
				vkCmdDraw(*cmdBuffer, vertexInput.getAttributeCount(0), 1, 0, 0);
			commandBufferEndRenderPass(rpbi);
			commandBufferMakeImagePresentationReady(cmdBuffer, framebufferGetImage(framebuffer));
		commandBufferEnd(cmdBuffer);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(ui.drawTrigger), (config.ticks ? onIdle : Canvas::OnIdle()));
}

} // unnamed namespace

template<> struct TestRecorder<FRACTALS>
{
	static bool record (TestRecord&);
};
bool TestRecorder<FRACTALS>::record (TestRecord& record)
{
	record.name = "fractals";
	record.call = &prepareTest;
	return true;
}
