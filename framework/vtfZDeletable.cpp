#include "GLFW/glfw3.h"
#include "vtfZDeletable.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfVkUtils.hpp"

#include <sstream>
#include <string_view>

static bool backtraceEnabled__ = false;
bool backtraceEnabled () { return backtraceEnabled__; }
void backtraceEnabled (bool enable) { backtraceEnabled__ = enable; }

VkAllocationCallbacks* getAllocationCallbacks()
{
	return nullptr;
}

void writeExpression (
	add_ref<std::ostringstream> ss,
	const char*	func,
	const char*	file,
	int			line,
    const char*	expr,
	VkResult	res,
	int			ind)
{
	ss << "ASSERT: In \"" << func << "\"" << std::endl;
	ss << "  from \"" << file << "\" at " << line << " line" << std::endl;
	if (expr) ss << std::string(std::string::size_type(ind), ' ') << "Expr: " << std::quoted(expr);
	if (VK_SUCCESS != res) ss << " returned " << vtf::vkResultToString(res);
	ss << std::endl;
}

void writeBackTrace (add_ref<std::ostringstream> ss)
{
#if SYSTEM_OS_LINUX == 1
	if (backtraceEnabled__) printBacktrace<20>(ss, 2);
#elif SYSTEM_OS_WINDOWS
	if (backtraceEnabled__) PrintStackTrace(ss, 20);
#endif
}

void deletable_selfTest() {}

void freeWindow(add_ptr<GLFWwindow> window)
{
	glfwDestroyWindow(window);
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Calling " << __func__ << ' ' << window << std::endl;
	}
}

void vtfDestroyInstance (VkInstance instance, VkAllocationCallbacksPtr callbacks)
{
	PFN_vkDestroyInstance proc = getDriverDestroyInstanceProc();
	if (proc)
	{
		(*proc)(instance, callbacks);
	}
}

void vtfDestroyDevice (VkDevice dev, VkAllocationCallbacksPtr cb, add_cref<ZPhysicalDevice> phd)
{
	UNREF(phd);
	PFN_vkDestroyDevice proc = getDriverDestroyDeviceProc();
	if (proc)
	{
		(*proc)(dev, cb);
	}
}

