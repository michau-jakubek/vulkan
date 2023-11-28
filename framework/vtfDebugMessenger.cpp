#include "vtfVkUtils.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"
#include "vtfThreadSafeLogger.hpp"

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

void getDebugCreateInfo (VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext, bool enableDebugPrintf)
{
	VkDebugUtilsMessageSeverityFlagsEXT noneDebugPrintfSeverityFlags = 0
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	result.sType			= VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	result.pNext			= pNext;
	result.flags			= 0;

	result.messageSeverity	= enableDebugPrintf
								? VkDebugUtilsMessageSeverityFlagsEXT(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
								: noneDebugPrintfSeverityFlags;
	result.messageType		= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
								| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
								| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	result.messageType		|= enableDebugPrintf ? VK_DEBUG_REPORT_INFORMATION_BIT_EXT : 0;
	result.pfnUserCallback	= debugMessengerCallback;
	result.pUserData		= pUserData;
}

void createDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, const VkDebugUtilsMessengerCreateInfoEXT& info, VkDebugUtilsMessengerEXT& messenger)
{
	auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
	ASSERTION(createDebugUtilsMessengerEXT != nullptr);

	ASSERTION(messenger == VK_NULL_HANDLE);
	VKASSERT2(createDebugUtilsMessengerEXT(*instance, &info, callbacks, &messenger));
}

void destroyDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugUtilsMessengerEXT& messenger)
{
	if (messenger != VK_NULL_HANDLE)
	{
		auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkDestroyDebugUtilsMessengerEXT");
		ASSERTION(destroyDebugUtilsMessengerEXT != nullptr);
		ASSERTION(messenger != VK_NULL_HANDLE);
		destroyDebugUtilsMessengerEXT(*instance, messenger, callbacks);
		messenger = VK_NULL_HANDLE;
	}
}

void getDebugCreateInfo (VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext, bool enableDebugPrintf)
{
	result.sType	= VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	result.pNext	= pNext;
	result.flags	= VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	result.flags	= VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	result.flags	|= (enableDebugPrintf ? VK_DEBUG_REPORT_INFORMATION_BIT_EXT : 0);
	result.pfnCallback	= debugReportCallback;
	result.pUserData	= pUserData;
}

void createDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, const VkDebugReportCallbackCreateInfoEXT& info, VkDebugReportCallbackEXT& report)
{
	auto createDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(*instance, "vkCreateDebugReportCallbackEXT");
	ASSERTION(createDebugReportCallbackEXT != nullptr);

	ASSERTION(report == VK_NULL_HANDLE);
	VKASSERT2(createDebugReportCallbackEXT(*instance, &info, callbacks, &report));
}

void destroyDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugReportCallbackEXT& report)
{
	if (report != VK_NULL_HANDLE)
	{
		auto destroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(*instance, "vkDestroyDebugReportCallbackEXT");
		ASSERTION(destroyDebugReportCallbackEXT != nullptr);
		ASSERTION(report != VK_NULL_HANDLE);
		destroyDebugReportCallbackEXT(*instance, report, callbacks);
		report = VK_NULL_HANDLE;
	}
}

static const char* VUID_Undefined = "VUID_Undefined";

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
	
	if (getGlobalAppFlags().noWarning_VUID_Undefined && (std::strcmp(pCallbackData->pMessageIdName, VUID_Undefined) == 0))
	{
		return VK_FALSE;
	}
	for (add_cref<std::string> s : getGlobalAppFlags().suppressedVUIDs)
	{
		if (s.length() > 5 && std::strncmp(s.c_str(), pCallbackData->pMessageIdName, s.length()) == 0)
			return VK_FALSE;
	}
	std::cout << "[VL]: " << pCallbackData->pMessage <<  std::endl;
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

	if (getGlobalAppFlags().noWarning_VUID_Undefined
		&& (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		&& (std::strstr(pMessage, VUID_Undefined) != nullptr))
	{
		return VK_FALSE;
	}

	for (add_cref<std::string> s : getGlobalAppFlags().suppressedVUIDs)
	{
		if (s.length() > 5 && std::strstr(s.c_str(), pMessage) != nullptr)
			return VK_FALSE;
	}

	std::string prefix = "[WARNING]";
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		prefix = "[ERROR]";
	else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
		prefix = "[WARNING]";
	else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		prefix = "[INFO]";
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		prefix = "[DEBUG]";


	/*if (pUserData)
	{
		add_ref<Logger> log = static_cast<add_ref<Logger>>(*static_cast<add_ptr<Logger>>(pUserData));
		log << prefix << " " << pLayerPrefix << ": " << pMessage << std::endl;
	}
	else*/
	std::cout << prefix << " " << pLayerPrefix << ": " << pMessage << std::endl;

	return VK_FALSE;
}

} // namespace vtf
