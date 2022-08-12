#ifndef __VTF_GLFW_EVENTS_HPP_INCLUDED__
#define __VTF_GLFW_EVENTS_HPP_INCLUDED__

#include <functional>
#include <tuple>
#include "vtfCanvas.hpp"

namespace vtf
{

template<class R, class... Args>
struct routine_signature
{
	typedef std::tuple<Args...> ArgList;
};
template<class R, class... Args> struct routine_t;
template<class R, class... Args> struct routine_t<R(Args...)>
	: routine_signature<R, Args...> { };

template<class routine_type__, std::size_t at__>
using routine_arg_t = std::tuple_element_t<at__, typename routine_t<routine_type__>::ArgList>;

template<class GLFWSetCallback, class... X>
struct CanvasEvent
{
	typedef void* UserData;
	typedef std::function<void(Canvas&, void*, X... params)> Callback;

	CanvasEvent (Canvas& canvas, GLFWSetCallback* setCallback, routine_arg_t<GLFWSetCallback, 1> callCallback)
		: callback			(m_callback)
		, userData			(m_userData)
		, enabled			(m_enabled)
		, m_canvas			(canvas)
		, m_callback		()
		, m_userData		(nullptr)
		, m_enabled			(false)
		, m_setCallback		(setCallback)
		, m_callCallback	(callCallback)
	{
		ASSERTION(setCallback);
		ASSERTION(callCallback);
	}
	const Callback&	callback;
	const UserData&	userData;
	const bool&		enabled;
	void set (Callback aCallback, UserData anUserData)
	{
		m_callback	= aCallback;
		m_userData	= anUserData;
		m_enabled	= true;
		(*m_setCallback)(*m_canvas.window, m_callCallback);
	}
	void enable ()
	{
		ASSERTION(m_callback);
		m_enabled = true;
		(*m_setCallback)(*m_canvas.window, m_callCallback);
	}
	void disable ()
	{
		m_enabled	= false;
		(*m_setCallback)(*m_canvas.window, nullptr);
	}

private:
	Canvas&								m_canvas;
	Callback							m_callback;
	UserData							m_userData;
	bool								m_enabled;
	GLFWSetCallback*					m_setCallback;
	routine_arg_t<GLFWSetCallback, 1>	m_callCallback;
};

struct GLFWEvents
{
	GLFWEvents (Canvas& canvas);
	CanvasEvent<decltype(glfwSetCursorPosCallback), double, double>		cbCursorPos;
	CanvasEvent<decltype(glfwSetScrollCallback), double, double>		cbScroll;
	CanvasEvent<decltype(glfwSetKeyCallback), int, int, int, int>		cbKey;
	CanvasEvent<decltype(glfwSetMouseButtonCallback), int, int, int>	cbMouseButton;
	CanvasEvent<decltype(glfwSetWindowSizeCallback), int, int>			cbWindowSize;

private:
	static void onCursorPos (GLFWwindow* window, double xpos, double ypos);
	static void onScroll	(GLFWwindow* window, double xoffset, double yoffset);
	static void onKey		(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void onMuseBtn	(GLFWwindow* window, int button, int action, int mods);
	static void onResize	(GLFWwindow* window, int width, int height);

	Canvas&	m_canvas;
};

} // namespace vtf

#endif // __VTF_GLFW_EVENTS_HPP_INCLUDED__
