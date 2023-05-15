
#include "allTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfPipelineLayout.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfBacktrace.hpp"

#include <variant>

using namespace vtf;

namespace
{

bool userEnforcesFloat32 (const strings& params)
{
	strings				sink;
	strings				args(params);
	Option				enforceFloate32	{ "-float32", 0 };
	std::vector<Option>	options { enforceFloate32 };
	return consumeOptions(enforceFloate32, options, args, sink) > 0;
}

int performTest (Canvas& canvas, const std::string&	assets, bool enableFloat64, const GlobalAppFlags& flags);

int prepareTest (const TestRecord& record, const strings& params)
{
	UNREF(params);

	std::cout << "Arguments"														<< std::endl;
	std::cout << "  [-float32] Enforce using Float32 in shader"							<< std::endl;
	std::cout << "  Otherwise Float64 will be used if available"					<< std::endl;
	std::cout																		<< std::endl;
	std::cout << "Navigation keys combination"										<< std::endl;
	std::cout << "  Scroll Left|Right|Up|Down: Move the picture slowly"				<< std::endl;
	std::cout << "  Ctrl or|and Mice Button pressed:"								<< std::endl;
	std::cout << "   * cursor move:            Move the picture fast"				<< std::endl;
	std::cout << "   * scroll dowm:            Zoom in"								<< std::endl;
	std::cout << "   * scroll dowm:            Zoom out"							<< std::endl;
	std::cout << "   (Mice's buttons might not work correctly on some platforms)"	<< std::endl;
	std::cout << "  R:                         Restore to start settings"			<< std::endl;
	std::cout << "  S:                         Print diagnostic information"		<< std::endl;
	std::cout << "  Gray(+)                    Increment iteration count"			<< std::endl;
	std::cout << "  Gray(-)                    Decrement iteration count"			<< std::endl;
	std::cout << "  Esc:                       Quit"								<< std::endl;

	bool	enableFloat64 = false;
	auto onGetEnabledFeatures = [&](ZPhysicalDevice physDevice, add_ref<strings> /*deviceExtensions*/) -> VkPhysicalDeviceFeatures2
	{
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(*physDevice, &features);

		VkPhysicalDeviceFeatures2 result{};
		result.features.shaderFloat64 = features.shaderFloat64;
		enableFloat64 = !userEnforcesFloat32(params) && (features.shaderFloat64 != VK_FALSE);

		return result;;
	};

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	Canvas	cs(record.name, gf.layers, {}, {}, Canvas::DefaultStyle, onGetEnabledFeatures, false, gf.apiVer);

	std::cout << "Current shader mode Float" << (enableFloat64 ? "64" : "32") << std::endl;

	return performTest(cs, record.assets, enableFloat64, gf);
}

template<class Float>
struct PushConstant
{
	Float width;
	Float height;
	Float xMin;
	Float xMax;
	Float yMin;
	Float yMax;
	int   iterCount;
};

struct UserInput_
{
	int					drawTrigger;
	double				xCursor;
	double				yCursor;
	bool				ctrlPressed;
	bool				micePressed;
	double				miceKeyXcursor;
	double				miceKeyYcursor;
	const int			iterationCount;
	const int			iterationStep;
	const bool			adaptiveIteration;
	int					iterationTick;
	UserInput_ ()
		: drawTrigger		(1)
		, xCursor			()
		, yCursor			()
		, ctrlPressed		()
		, micePressed		()
		, miceKeyXcursor	()
		, miceKeyYcursor	()
		, iterationCount	(50)
		, iterationStep		(20)
		, adaptiveIteration	(true)
		, iterationTick		(1)
	{
	}
	virtual void reset (Canvas& cs) = 0;
	virtual void updateDim (Canvas& cs) = 0;
};

template<class Float>
struct UserInput : UserInput_
{
	PushConstant<Float>	pc;
	UserInput() : pc() {}
	void reset (Canvas& cs) override
	{
		pc.iterCount = iterationCount;
		updateDim(cs);
		pc.xMin = -2.5f;
		pc.xMax = +1.5f;
		pc.yMin = -2.0f;
		pc.yMax = +2.0f;
		ctrlPressed = false;
		micePressed	= false;
		glfwGetCursorPos(*cs.window, &xCursor, &yCursor);
	}
	void updateDim (Canvas& cs) override
	{
		pc.width = Float(cs.swapchain.extent.width);
		pc.height = Float(cs.swapchain.extent.height);
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
	if (key == GLFW_KEY_LEFT_CONTROL)
	{
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->ctrlPressed = (GLFW_PRESS == action || GLFW_REPEAT == action);
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
		ui->micePressed = action == GLFW_PRESS;
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

void initEvents (Canvas& cs, VarUserInput& vui, bool enableFloat64)
{
	if (enableFloat64)
	{
		cs.events().cbWindowSize.set(onResize<double>,		&std::get<UserInput<double>>(vui));
		cs.events().cbCursorPos.set(onCursorPos<double>,	&std::get<UserInput<double>>(vui));
		cs.events().cbScroll.set(onScroll<double>,			&std::get<UserInput<double>>(vui));
		cs.events().cbMouseButton.set(onMouseBtn<double>,	&std::get<UserInput<double>>(vui));
		cs.events().cbKey.set(onKey<double>,				&std::get<UserInput<double>>(vui));
	}
	else
	{
		cs.events().cbWindowSize.set(onResize<float>,		&std::get<UserInput<float>>(vui));
		cs.events().cbCursorPos.set(onCursorPos<float>,		&std::get<UserInput<float>>(vui));
		cs.events().cbScroll.set(onScroll<float>,			&std::get<UserInput<float>>(vui));
		cs.events().cbMouseButton.set(onMouseBtn<float>,	&std::get<UserInput<float>>(vui));
		cs.events().cbKey.set(onKey<float>,					&std::get<UserInput<float>>(vui));
	}
}

using namespace std::placeholders;
void commandBufferPushConstants (ZCommandBuffer cmdBuffer, ZPipelineLayout pipelineLayout,
								 const VarUserInput& vui, bool enableFloat64)
{
	if (enableFloat64)
		::vtf::commandBufferPushConstants(cmdBuffer, pipelineLayout, std::get<UserInput<double>>(vui).pc);
	else ::vtf::commandBufferPushConstants(cmdBuffer, pipelineLayout, std::get<UserInput<float>>(vui).pc);
}

int performTest (Canvas& cs, const std::string&	assets, bool enableFloat64, const GlobalAppFlags& flags)
{
	ProgramCollection		programs(cs);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, (assets + "shader.vert"));
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, (assets + (enableFloat64 ? "dshader.frag" : "fshader.frag")));
	programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate);

	ZShaderModule			vertShaderModule	= *programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
	ZShaderModule			fragShaderModule	= *programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

	// Scope destruction of a vector
	{
		std::vector<Vec2>	vertices;

		vertices.emplace_back(-1, -1);
		vertices.emplace_back(+1, -1);
		vertices.emplace_back(-1, +1);
		vertices.emplace_back(-1, +1);
		vertices.emplace_back(+1, -1);
		vertices.emplace_back(+1, +1);

		cs.vertexInput.binding(0).addAttributes(vertices);
	}

	PipelineLayout			pm					(cs);
	const VkClearValue		clearColor			= {{{0.5f, 0.5f, 0.5, 0.5f}}};
	ZRenderPass				renderPass			= cs.createRenderPass({cs.format.format}, clearColor);
	ZPipelineLayout			pipelineLayout		= enableFloat64
													? pm.createPipelineLayout<PushConstant<double>>()
													: pm.createPipelineLayout<PushConstant<float>>();
	ZPipeline				pipeline			= cs.createGraphicsPipeline(pipelineLayout, renderPass, {},
																			vertShaderModule, fragShaderModule);
	VarUserInput			vui					= selectUserInputVariant(enableFloat64);
	add_ref<UserInput_>		ui					= enableFloat64
													? static_cast<add_ref<UserInput_>>(std::get<UserInput<double>>(vui))
													: static_cast<add_ref<UserInput_>>(std::get<UserInput<float>>(vui));
	ui.reset(cs);

	initEvents(cs, vui, enableFloat64);

	auto onCommandRecording = [&](Canvas&, ZCommandBuffer cmdBuffer, uint32_t swapImageIndex)
	{
		ui.updateDim(cs);

		const VkRenderPassBeginInfo rpbi = cs.makeRenderPassBeginInfo(renderPass, swapImageIndex);

		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, pipeline);
			commandBufferBindVertexBuffers(cmdBuffer, cs.vertexInput);
			commandBufferPushConstants(cmdBuffer, pipelineLayout, vui, enableFloat64);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &cs.swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &cs.swapchain.scissor);
			vkCmdBeginRenderPass(*cmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdDraw(*cmdBuffer, cs.vertexInput.getAttributeCount(0), 1, 0, 0);
			vkCmdEndRenderPass(*cmdBuffer);
			cs.transitionImageForPresent(cmdBuffer, swapImageIndex);
		commandBufferEnd(cmdBuffer);

		return true;
	};

	return cs.run(renderPass, onCommandRecording, std::ref(ui.drawTrigger));
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
