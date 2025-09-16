#include "GLFW/glfw3.h"
#include "vtfZDeletable.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfVkUtils.hpp"

#include <iterator>
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

void freeWindow (add_ptr<GLFWwindow> window)
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

void freePipelineCache(VkDevice device, VkPipelineCache cache, VkAllocationCallbacksPtr callbacks,
	add_cref<std::string> cacheFileName)
{
	size_t dataSize = 0u;
	std::vector<char> data;
	VKASSERT(vkGetPipelineCacheData(device, cache, &dataSize, nullptr));
	if (dataSize)
	{
		data.resize(dataSize);
		VKASSERT(vkGetPipelineCacheData(device, cache, &dataSize, data.data()));

		add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
		const fs::path tmpPath = std::strlen(gf.tmpDir) ? fs::path(gf.tmpDir) : fs::temp_directory_path();
		const fs::path cachePath = tmpPath / cacheFileName;
		std::basic_ofstream<char> file(cachePath.c_str(), std::ios::binary);
		if (file.is_open())
		{

			std::copy(data.begin(), data.end(), std::ostream_iterator<char>(file));
			file.close();
		}
	}

	vkDestroyPipelineCache(device, cache, callbacks);
}
