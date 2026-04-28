#include "GLFW/glfw3.h"
#include "vtfZDeletable.hpp"
#include "vtfBacktrace.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfPlatformDriver.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfZPipeline.hpp"

#include <iterator>
#include <sstream>
#include <string_view>

using namespace vtf;

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

void throwCorrectException (VkResult res, add_cref<std::string> msg)
{
	if (VK_SUCCESS == res)
		throw std::runtime_error(msg);
	else
		throw vtf::ZVulkanResultException(res, msg);
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

void vtfDestroyInstance (VkInstance instance, VkAllocationCallbacksPtr callbacks, uint32_t undeletable)
{
	if (undeletable) return;
	add_cref<ZInstanceInterface> ii = ZInstanceSingleton().getInterface();
	VTF_CALL_CHECK(ii.vkDestroyInstance, instance, callbacks);
}

void vtfDestroySurfaceKHR(void_cptr obj, VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* callbacks)
{
	add_cref<ZInstanceInterface> ii = reinterpret_cast<add_cptr<ZInstance>>(obj)->getInterface();
	VTF_CALL_CHECK(ii.vkDestroySurfaceKHR, instance, surface, callbacks);
}

void vtfDestroyDevice (VkDevice dev, VkAllocationCallbacksPtr cb, uint32_t undeletable)
{
	if (undeletable) return;
	add_cref<ZDeviceInterface> di = ZDeviceSingleton().getInterface();
	VTF_CALL_CHECK(di.vkDestroyDevice, dev, cb);
}

void vtfDestroySwapchainKHR(void_cptr obj, VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* callbacks)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroySwapchainKHR, device, swapchain, callbacks);
}

void vtfDestroyShaderModule(void_cptr obj, VkDevice device, VkShaderModule module, const VkAllocationCallbacks* callbacks)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyShaderModule, device, module, callbacks);
}

void vtfDestroyCommandPool(void_cptr obj, VkDevice device, VkCommandPool pool, const VkAllocationCallbacks* callbacks)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyCommandPool, device, pool, callbacks);
}

void freeCommandBuffer(void_cptr obj, VkDevice device, VkCommandBuffer buffer, VkCommandPool pool)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkFreeCommandBuffers, device, pool, 1u, &buffer);
}

void vtfDestroyFence(void_cptr obj, VkDevice device, VkFence fence, const VkAllocationCallbacks* callbacks)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyFence, device, fence, callbacks);
}

void vtfDestroySemaphore(void_cptr obj, VkDevice dev, VkSemaphore sem, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroySemaphore, dev, sem, cbs);
}

void vtfFreeMemory(void_cptr obj, VkDevice dev, VkDeviceMemory mem, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkFreeMemory, dev, mem, cbs);
}

void vtfDestroyImage(void_cptr obj, VkDevice dev, VkImage img, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyImage, dev, img, cbs);
}

void vtfDestroyImageView(void_cptr obj, VkDevice dev, VkImageView view, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyImageView, dev, view, cbs);
}

void vtfDestroyRenderPass(void_cptr obj, VkDevice dev, VkRenderPass rp, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyRenderPass, dev, rp, cbs);
}

void vtfDestroyFramebuffer(void_cptr obj, VkDevice dev, VkFramebuffer fb, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyFramebuffer, dev, fb, cbs);
}

void vtfDestroySampler(void_cptr obj, VkDevice dev, VkSampler smp, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroySampler, dev, smp, cbs);
}

void vtfDestroyBuffer(void_cptr obj, VkDevice dev, VkBuffer buff, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyBuffer, dev, buff, cbs);
}

void vtfDestroyDescriptorPool(void_cptr obj, VkDevice dev, VkDescriptorPool dp, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyDescriptorPool, dev, dp, cbs);
}

void freeDescriptorSet(void_cptr obj, VkDevice dev, VkDescriptorSet set, VkDescriptorPool pool)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
    VTF_CALL_CHECK(di.vkFreeDescriptorSets, dev, pool, 1u, &set);
}

void vtfDestroyDescriptorSetLayout(void_cptr obj, VkDevice dev, VkDescriptorSetLayout dsl, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyDescriptorSetLayout, dev, dsl, cbs);
}

void vtfDestroyPipelineLayout(void_cptr obj, VkDevice dev, VkPipelineLayout pl, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyPipelineLayout, dev, pl, cbs);
}

void vtfDestroyPipeline(void_cptr obj, VkDevice dev, VkPipeline pl, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyPipeline, dev, pl, cbs);
}

void ZPipelineCacheBase::saveToFile ()
{
	auto cache = static_cast<add_ptr<ZPipelineCache>>(this);
	ZDevice device = cache->getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	add_cref<std::string> cacheFileName = cache->getParamRef<std::string>();

	size_t dataSize = 0u;
	VKASSERT(VTF_CALL_CHECK(di.vkGetPipelineCacheData, *device, cache->operator*(), &dataSize, nullptr));

	ASSERTMSG(dataSize, "Pipeline Cache Data must not be 0");
	std::vector<char> data(dataSize);
	VKASSERT(VTF_CALL_CHECK(di.vkGetPipelineCacheData, *device, cache->operator*(), &dataSize, data.data()));

	std::ofstream file(cacheFileName, std::ios::binary);
	ASSERTMSG(file.is_open(), "Unable to open \"", cacheFileName, "\" for writting");

    file.write(data.data(), std::streamsize(dataSize));
	file.close();
}
void freePipelineCache(ZDevice device, VkPipelineCache cache, add_cref<std::string> cacheFileName, bool saveOnDestroy)
{
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();
	if (saveOnDestroy)
	{
		ZPipelineCache pc(cache, device, callbacks, cacheFileName, false);
		pc.saveToFile();
		cache = VK_NULL_HANDLE;
	}
	add_cref<ZDeviceInterface> di = device.getInterface();
	VTF_CALL_CHECK(di.vkDestroyPipelineCache, *device, cache, callbacks);
}

void vtfDestroyQueryPool(void_cptr obj, VkDevice dev, VkQueryPool qp, const VkAllocationCallbacks* cbs)
{
	add_cref<ZDeviceInterface> di = reinterpret_cast<add_cptr<ZDevice>>(obj)->getInterface();
	VTF_CALL_CHECK(di.vkDestroyQueryPool, dev, qp, cbs);
}
