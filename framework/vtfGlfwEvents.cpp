#include "vtfGlfwEvents.hpp"
#include "vtfCanvas.hpp"

namespace vtf
{

GLFWEvents::GLFWEvents (Canvas& canvas)
	: cbCursorPos	(canvas, *this, &glfwSetCursorPosCallback, &GLFWEvents::onCursorPos)
	, cbScroll		(canvas, *this, &glfwSetScrollCallback, &GLFWEvents::onScroll)
	, cbKey			(canvas, *this, &glfwSetKeyCallback, &GLFWEvents::onKey)
	, cbMouseButton	(canvas, *this, &glfwSetMouseButtonCallback, &GLFWEvents::onMouseBtn)
	, cbWindowSize	(canvas, *this, &glfwSetWindowSizeCallback, &GLFWEvents::onResize)
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

namespace default_callbacks
{

static void onKey (add_ref<Canvas> cs, void* userData, const int key, int scancode, int action, int mods)
{
	MULTI_UNREF(scancode, mods);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		*((int*)userData) += 1;
	}
}

static void onResize (add_ref<Canvas> cs, void* userData, int width, int height)
{
	MULTI_UNREF(cs, width, height);
	if (userData)
	{
		*((int*)userData) += 1;
	}
}

} // namespace default_callbacks

void GLFWEvents::setDefault (add_ref<int> drawTrigger)
{
	// TODO: povide and reset m_default flag

	cbKey.set(default_callbacks::onKey, &drawTrigger);
	cbWindowSize.set(default_callbacks::onResize, &drawTrigger);
}

} // namespace vtf
