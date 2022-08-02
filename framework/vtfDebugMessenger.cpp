#include "vtfVkUtils.hpp"
#include "vtfDebugMessenger.hpp"

namespace vtf
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT				messageType,
	const VkDebugUtilsMessengerCallbackDataEXT*	pCallbackData,
	void*										pUserData);

static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	objectType,
	uint64_t					object,
	size_t						location,
	int32_t						messageCode,
	const char*					pLayerPrefix,
	const char*					pMessage,
	void*						pUserData);

void getDebugCreateInfo (VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext)
{
	result.sType			= VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	result.pNext			= pNext;
	result.flags			= 0;
	result.messageSeverity	= 0
								//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
								| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
								| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	result.messageType		= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
									| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
									| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	result.pfnUserCallback	= debugMessengerCallback;
	result.pUserData		= pUserData;
}

void createDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, void* pUserData, VkDebugUtilsMessengerEXT& messenger)
{
	auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
	ASSERTION(createDebugUtilsMessengerEXT != nullptr);

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	getDebugCreateInfo(createInfo, pUserData);

	ASSERTION(messenger == VK_NULL_HANDLE);
	VKASSERT2(createDebugUtilsMessengerEXT(*instance, &createInfo, callbacks, &messenger));
}

void destroyDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugUtilsMessengerEXT& messenger)
{
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*instance, "vkDestroyDebugUtilsMessengerEXT");
	ASSERTION(destroyDebugUtilsMessengerEXT != nullptr);
	ASSERTION(messenger != VK_NULL_HANDLE);
	destroyDebugUtilsMessengerEXT(*instance, messenger, callbacks);
	messenger = VK_NULL_HANDLE;
}

void getDebugCreateInfo (VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext)
{
	result.sType	= VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	result.pNext	= pNext;
	result.flags	= VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	result.flags	= VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	result.pfnCallback	= debugReportCallback;
	result.pUserData	= pUserData;
}

void createDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, void* pUserData, VkDebugReportCallbackEXT& report)
{
	auto createDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(*instance, "vkCreateDebugReportCallbackEXT");
	ASSERTION(createDebugReportCallbackEXT != nullptr);

	VkDebugReportCallbackCreateInfoEXT debugReportInfo{};
	getDebugCreateInfo(debugReportInfo, pUserData);

	ASSERTION(report == VK_NULL_HANDLE);
	VKASSERT2(createDebugReportCallbackEXT(*instance, &debugReportInfo, callbacks, &report));
}

void destroyDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugReportCallbackEXT& report)
{
	auto destroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(*instance, "vkDestroyDebugReportCallbackEXT");
	ASSERTION(destroyDebugReportCallbackEXT != nullptr);
	ASSERTION(report != VK_NULL_HANDLE);
	destroyDebugReportCallbackEXT(*instance, report, callbacks);
	report = VK_NULL_HANDLE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT				messageType,
	const VkDebugUtilsMessengerCallbackDataEXT*	pCallbackData,
	void*										pUserData)
{
	UNREF(messageSeverity);
	UNREF(messageType);
	UNREF(pCallbackData);
	UNREF(pUserData);
	std::cout << "[VL]: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	objectType,
	uint64_t					object,
	size_t						location,
	int32_t						messageCode,
	const char*					pLayerPrefix,
	const char*					pMessage,
	void*						pUserData)
{
	UNREF(objectType);
	UNREF(object);
	UNREF(location);
	UNREF(messageCode);
	UNREF(pUserData);

	std::string prefix = "[WARNING]";
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		prefix = "[ERROR]";
	else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
		prefix = "[WARNING]";
	else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		prefix = "[INFO]";
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		prefix = "[DEBUG]";

	std::cout << prefix << " " << pLayerPrefix << ": " << pMessage << std::endl;

	return VK_FALSE;
}

} // namespace vtf
