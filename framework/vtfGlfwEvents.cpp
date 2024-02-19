#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"

namespace vtf
{

GLFWEvents::GLFWEvents (Canvas& canvas)
	: cbCursorPos	(canvas, &glfwSetCursorPosCallback, &GLFWEvents::onCursorPos)
	, cbScroll		(canvas, &glfwSetScrollCallback, &GLFWEvents::onScroll)
	, cbKey			(canvas, &glfwSetKeyCallback, &GLFWEvents::onKey)
	, cbMouseButton	(canvas, &glfwSetMouseButtonCallback, &GLFWEvents::onMouseBtn)
	, cbWindowSize	(canvas, &glfwSetWindowSizeCallback, &GLFWEvents::onResize)
{
}

void GLFWEvents::onCursorPos (GLFWwindow* window, double x, double y)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(glfwGetWindowUserPointer(window));
	if (canvas->events().cbCursorPos.callback)
	{
		canvas->events().cbCursorPos.callback(*canvas, canvas->events().cbCursorPos.userData,
											  static_cast<float>(x), static_cast<float>(y));
	}
}

void GLFWEvents::onScroll (GLFWwindow* window, double xoffset, double yoffset)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(glfwGetWindowUserPointer(window));
	if (canvas->events().cbScroll.callback)
	{
		canvas->events().cbScroll.callback(*canvas, canvas->events().cbScroll.userData,
										   xoffset, yoffset);
	}
}

void GLFWEvents::onKey (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(glfwGetWindowUserPointer(window));
	if (canvas->events().cbKey.callback)
	{
		canvas->events().cbKey.callback(*canvas, canvas->events().cbKey.userData,
										key, scancode, action, mods);
	}
}

void GLFWEvents::onMouseBtn (GLFWwindow* window,  int button, int action, int mods)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(glfwGetWindowUserPointer(window));
	if (canvas->events().cbMouseButton.callback)
	{
		canvas->events().cbMouseButton.callback(*canvas, canvas->events().cbMouseButton.userData,
										button, action, mods);
	}
}

void GLFWEvents::onResize (GLFWwindow* window, int width, int height)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(glfwGetWindowUserPointer(window));
	if (canvas->events().cbWindowSize.callback)
	{
		canvas->events().cbWindowSize.callback(*canvas, canvas->events().cbWindowSize.userData,
										width, height);
	}
}

} // namespace vtf
