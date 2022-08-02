
#include "allTests.hpp"
#include "vtfCanvas.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgramCollection.hpp"
#include "vtfPipelineLayout.hpp"
#include "vtfZCommandBuffer.hpp"

using namespace vtf;

namespace
{

int performTest (Canvas& canvas, const std::string&	assets);

int prepareTest (const TestRecord& record, const strings& params)
{
	UNREF(params);

	std::cout << "Navigation keys combination" << std::endl;
	std::cout << "  Scroll Left|Right|Up|Down: Move the picture slowly" << std::endl;
	std::cout << "  Ctrl or|and Mice Button &&" << std::endl;
	std::cout << "   * cursor move:            Move the picture fast" << std::endl;
	std::cout << "   * scroll dowm:            Zoom in" << std::endl;
	std::cout << "   * scroll dowm:            Zoom out" << std::endl;
	std::cout << "  R:                         Restore to start settings" << std::endl;
	std::cout << "  Esc:                       Quit" << std::endl;

	Canvas	cs (record.name,
				record.layers, {}, {},
				VK_API_VERSION_1_0,
				VK_MAKE_VERSION(1, 0, 0),
				VK_MAKE_VERSION(1, 0, 0),
				Canvas::DefaultStyle,
				2);

	return performTest(cs, record.assets);
}

struct PushConstant
{
	float width;
	float height;
	float xMin;
	float xMax;
	float yMin;
	float yMax;
	int   iterCount;
};

struct UserInput
{
	int				drawTrigger;
	double			xCursor;
	double			yCursor;
	bool			ctrlPressed;
	bool			micePressed;
	double			miceKeyXcursor;
	double			miceKeyYcursor;
	PushConstant	pc;
	const int		iterationCount;
	const int		iterationStep;
	const bool		adaptiveIteration;
	int				iterationTick;
	UserInput()
		: drawTrigger		(1)
		, xCursor			()
		, yCursor			()
		, ctrlPressed		()
		, micePressed		()
		, miceKeyXcursor	()
		, miceKeyYcursor	()
		, pc				()
		, iterationCount	(50)
		, iterationStep		(20)
		, adaptiveIteration	(true)
		, iterationTick		(1)
	{
	}
	void reset (Canvas& cs)
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
	void updateDim (Canvas& cs)
	{
		pc.width = float(cs.swapchain.extent.width);
		pc.height = float(cs.swapchain.extent.height);
	}
	void incIterationCount (int step = 0, uint32_t tick = 1)
	{
		if (iterationTick + 1 == std::numeric_limits<int>::max())
			iterationTick = 0;
		else iterationTick = iterationTick + 1;

		step = !step ? iterationStep : std::abs(step);

		const bool inc = tick ? ((iterationTick % tick) == 0) : true;

		if (inc) pc.iterCount += step;
	}
	void decIterationCount (int step = 0, uint32_t tick = 1)
	{
		if (iterationTick + 1 == std::numeric_limits<int>::max())
			iterationTick = 0;
		else iterationTick = iterationTick + 1;

		step = !step ? iterationStep : std::abs(step);

		const bool inc = tick ? ((iterationTick % tick) == 0) : true;

		if (inc && pc.iterCount > iterationCount)
			pc.iterCount = std::max(iterationCount, (pc.iterCount - step));
	}
};

typedef Canvas::Area<float> ZArea;

ZArea areaFromPC (const PushConstant& pc)
{
	ZArea a;
	a.xMin = pc.xMin;
	a.xMax = pc.xMax;
	a.yMin = pc.yMin;
	a.yMax = pc.yMax;
	return a;
}

void areaToPC (const ZArea& a, PushConstant& pc)
{
	pc.xMin = a.xMin;
	pc.xMax = a.xMax;
	pc.yMin = a.yMin;
	pc.yMax = a.yMax;
}

void onResize (Canvas& canvas, void* userData, int width, int height)
{
	UNREF(canvas);
	UNREF(width);
	UNREF(height);
	UserInput* ui = reinterpret_cast<UserInput*>(userData);
	++ui->drawTrigger;
}

void onKey (Canvas& cs, void* userData, int key, int scancode, int action, int mods)
{
	UNREF(scancode);
	UNREF(mods);
	UserInput* ui = reinterpret_cast<UserInput*>(userData);
	++ui->drawTrigger;
	if (key == GLFW_KEY_LEFT_CONTROL)
	{
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->ctrlPressed = action == GLFW_PRESS;
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

			case GLFW_KEY_KP_ADD:
				ui->incIterationCount();
				break;

			case GLFW_KEY_KP_SUBTRACT:
				ui->decIterationCount();
				break;
		}
	}
}

void onMouseBtn (Canvas&, void* userData, int button, int action, int mods)
{
	UNREF(mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		UserInput* ui = reinterpret_cast<UserInput*>(userData);
		++ui->drawTrigger;
		ui->miceKeyXcursor = ui->xCursor;
		ui->miceKeyYcursor = ui->yCursor;
		ui->micePressed = action == GLFW_PRESS;
	}
}

void onCursorPos (Canvas& canvas, void* userData, double xpos, double ypos)
{
	UNREF(canvas);

	UserInput* ui = reinterpret_cast<UserInput*>(userData);
	++ui->drawTrigger;

	ui->xCursor = xpos;
	ui->yCursor = ypos;

	if (ui->micePressed || ui->ctrlPressed)
	{
		double wx = ui->miceKeyXcursor - xpos;
		double wy = ypos - ui->miceKeyYcursor;
		Canvas::Area a = areaFromPC(ui->pc);
		float dx = (float(wx) * a.width()) / float(canvas.width);
		float dy = (float(wy) * a.height()) / float(canvas.height);
		a.move(dx, dy);
		areaToPC(a, ui->pc);
		ui->miceKeyXcursor = xpos;
		ui->miceKeyYcursor = ypos;
	}
}

void onScroll (Canvas& canvas, void* userData, double xScrollOffset, double yScrollOffset)
{
	auto allowScroll = [&](UserInput* ui) {
		return	(yScrollOffset != 0.0)
				&& (ui->xCursor >= 2.0 && ui->xCursor < double(canvas.width - 1))
				&& (ui->yCursor >= 2.0 && ui->yCursor < double(canvas.height - 1));
	};

	// xScrollOffset > 0 - scroll right
	// yScrollOffset > 0 - scroll down

	const float width = float(canvas.width);	UNREF(width);
	const float height = float(canvas.height);	UNREF(height);
	UserInput* ui = reinterpret_cast<UserInput*>(userData);
	++ui->drawTrigger;

	if (ui->ctrlPressed || ui->micePressed)
	{
		if (!allowScroll(ui)) return;

		float scale = 1.0f + 1.0f / 32.0f;
		if (yScrollOffset > 0.0)
		{
			scale = 1.0f / scale;

			if (ui->adaptiveIteration)
				ui->incIterationCount(8,10);
		}
		else if (ui->adaptiveIteration)
			ui->decIterationCount(8,10);

		Vec2 oldPoint;
		Canvas::Area a = areaFromPC(ui->pc);
		canvas.windowToUser(a, Vec2(ui->xCursor, (height - ui->yCursor)), oldPoint);

		a.scale(scale);
		Vec2 newPoint = oldPoint * scale;
		float xoffset = oldPoint.x() - newPoint.x();
		float yoffset = oldPoint.y() - newPoint.y();
		a.move(xoffset, yoffset);
		areaToPC(a, ui->pc);
	}
	else
	{
		const Canvas::Area a = areaFromPC(ui->pc);
		const float dx = (a.width() * 3.0f) / width;
		const float dy = (a.height() * 2.0f) / height;
		ui->pc.xMin -= static_cast<float>(dx * xScrollOffset);
		ui->pc.xMax -= static_cast<float>(dx * xScrollOffset);
		ui->pc.yMin += static_cast<float>(dy * yScrollOffset);
		ui->pc.yMax += static_cast<float>(dy * yScrollOffset);
	}
}

int performTest (Canvas& cs, const std::string&	assets)
{
	ProgramCollection		programs(cs);
	programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT, (assets + "shader.vert"));
	programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, (assets + "shader.frag"));
	programs.buildAndVerify();

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
	const VkClearValue		clearColor			= {0.5f, 0.5f, 0.5, 0.5f};
	ZRenderPass				renderPass			= cs.createRenderPass({cs.format.format}, clearColor);
	ZPipelineLayout			pipelineLayout		= pm.createPipelineLayout<PushConstant>();
	ZPipeline				pipeline			= cs.createGraphicsPipeline(pipelineLayout, renderPass, {/*dynamic viewport & scissor*/},
																			vertShaderModule, fragShaderModule);

	UserInput				ui;
	ui.reset(cs);

	cs.events().cbWindowSize.set(onResize, &ui);
	cs.events().cbCursorPos.set(onCursorPos, &ui);
	cs.events().cbScroll.set(onScroll, &ui);
	cs.events().cbMouseButton.set(onMouseBtn, &ui);
	cs.events().cbKey.set(onKey, &ui);

	auto onCommandRecording = [&](Canvas&, ZCommandBuffer cmdBuffer, uint32_t swapImageIndex)
	{
		ui.updateDim(cs);

		const VkRenderPassBeginInfo rpbi = cs.makeRenderPassBeginInfo(renderPass, swapImageIndex);

		commandBufferBegin(cmdBuffer);
			commandBufferBindPipeline(cmdBuffer, pipeline);
			commandBufferBindVertexBuffers(cmdBuffer, cs.vertexInput);
			commandBufferPushConstants(cmdBuffer, pipelineLayout, ui.pc);
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
