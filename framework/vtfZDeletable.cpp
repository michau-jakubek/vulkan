#include <sstream>
#include <string_view>
#include "GLFW/glfw3.h"
#include "vtfZDeletable.hpp"
#include "vtfBacktrace.hpp"

#if SYSTEM_OS_LINUX == 1
#include "vtfBacktrace.hpp"
#endif

static bool backtraceEnabled__ = false;
bool backtraceEnabled () { return backtraceEnabled__; }
void backtraceEnabled (bool enable) { backtraceEnabled__ = enable; }

VkAllocationCallbacks* getAllocationCallbacks()
{
	return nullptr;
}

void assertion (bool cond, const char* func, const char* file, int line, add_cref<std::string> msg)
{
	if (!cond)
	{
		std::stringstream ss;
		ss << "ASSERT: In \"" << func << "\"" << std::endl;
		ss << "  from \"" << file << "\" at " << line << " line" << std::endl;
		if (!msg.empty()) ss << "  \"" << msg << '\"' << std::endl;
#if SYSTEM_OS_LINUX == 1
		if (backtraceEnabled__) printBacktrace<20>(ss, 2);
#endif
		ss.flush();
		throw std::runtime_error(ss.str());
	}
}

void freeWindow(add_ptr<GLFWwindow> window)
{
	glfwDestroyWindow(window);
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Calling " << __func__ << ' ' << window << std::endl;
	}
}

void deletable_selfTest () {}
