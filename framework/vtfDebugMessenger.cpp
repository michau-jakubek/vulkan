#include "vtfVkUtils.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"
#include "vtfThreadSafeLogger.hpp"
#include "vtfStructUtils.hpp"

#include <string_view>

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

void makeDebugCreateInfo (VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext, bool enableDebugPrintf)
{
	VkDebugUtilsMessageSeverityFlagsEXT debugPrintfBit = enableDebugPrintf
			? VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : 0;
	VkDebugUtilsMessageSeverityFlagsEXT severityFlags = 0
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		| debugPrintfBit;

	VkDebugUtilsMessageTypeFlagsEXT	typeFlags =	VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
												| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
												| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	result = makeVkStruct(pNext);
	result.messageSeverity	= severityFlags;
	result.messageType		= typeFlags;
	result.pfnUserCallback	= debugMessengerCallback;
	result.pUserData		= pUserData;
}

void createDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, const VkDebugUtilsMessengerCreateInfoEXT& info, VkDebugUtilsMessengerEXT& messenger)
{
	auto createDebugUtilsMessengerEXT =	 (PFN_vkCreateDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT");
	ASSERTION(createDebugUtilsMessengerEXT != nullptr);

	ASSERTION(messenger == VK_NULL_HANDLE);
	VKASSERT(createDebugUtilsMessengerEXT(*instance, &info, callbacks, &messenger));
}

void destroyDebugMessenger (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugUtilsMessengerEXT& messenger)
{
	if (messenger != VK_NULL_HANDLE)
	{
		auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
				vkGetInstanceProcAddr(*instance, "vkDestroyDebugUtilsMessengerEXT");
		ASSERTION(destroyDebugUtilsMessengerEXT != nullptr);
		ASSERTION(messenger != VK_NULL_HANDLE);
		destroyDebugUtilsMessengerEXT(*instance, messenger, callbacks);

		if (getGlobalAppFlags().verbose)
		{
			std::cout << "[INFO] Calling vkDestroyDebugUtilsMessengerEXT "
					  << instance << " VkDebugUtilsMessengerEXT " << messenger << std::endl;
		}
	}
	messenger = VK_NULL_HANDLE;
}

void destroyDebugMessenger (ZInstance i)
{
	destroyDebugMessenger(i, i.getParam<VkAllocationCallbacksPtr>(), i.getParamRef<VkDebugUtilsMessengerEXT>());
}

void makeDebugCreateInfo (VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext, bool enableDebugPrintf)
{
	const VkDebugReportFlagsEXT debugPrintfBit = enableDebugPrintf ? VK_DEBUG_REPORT_INFORMATION_BIT_EXT : 0;
	result = makeVkStruct(pNext);
	result.flags	= debugPrintfBit
						| VK_DEBUG_REPORT_WARNING_BIT_EXT
						| VK_DEBUG_REPORT_ERROR_BIT_EXT
						| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	result.pfnCallback	= debugReportCallback;
	result.pUserData	= pUserData;
}

void createDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, const VkDebugReportCallbackCreateInfoEXT& info, VkDebugReportCallbackEXT& report)
{
	auto createDebugReportCallbackEXT =	 (PFN_vkCreateDebugReportCallbackEXT)
			vkGetInstanceProcAddr(*instance, "vkCreateDebugReportCallbackEXT");
	ASSERTION(createDebugReportCallbackEXT != nullptr);

	ASSERTION(report == VK_NULL_HANDLE);
	VKASSERT(createDebugReportCallbackEXT(*instance, &info, callbacks, &report));
}

void destroyDebugReport (ZInstance instance, VkAllocationCallbacksPtr callbacks, VkDebugReportCallbackEXT& report)
{
	if (report != VK_NULL_HANDLE)
	{
		auto destroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
				vkGetInstanceProcAddr(*instance, "vkDestroyDebugReportCallbackEXT");
		ASSERTION(destroyDebugReportCallbackEXT != nullptr);
		ASSERTION(report != VK_NULL_HANDLE);
		destroyDebugReportCallbackEXT(*instance, report, callbacks);

		if (getGlobalAppFlags().verbose)
		{
			std::cout << "[INFO] Calling vkDestroyDebugReportCallbackEXT "
					  << instance << " VkDebugReportCallbackEXT " << report << std::endl;
		}
	}
	report = VK_NULL_HANDLE;
}

void destroyDebugReport (ZInstance i)
{
	destroyDebugReport(i, i.getParam<VkAllocationCallbacksPtr>(), i.getParamRef<VkDebugReportCallbackEXT>());
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
	std::cout << "[MSG]: " << pCallbackData->pMessage <<  std::endl;
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

	const std::string_view sv(pMessage);

	if (getGlobalAppFlags().noWarning_VUID_Undefined
		&& (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		&& (sv.find(VUID_Undefined) != std::string_view::npos))
	{
		return VK_FALSE;
	}

	for (add_cref<std::string> s : getGlobalAppFlags().suppressedVUIDs)
	{
		if (s.length() > 5 && sv.find(s.c_str()) != std::string_view::npos)
			return VK_FALSE;
	}

	static add_cptr<char> const infoPrefix		= "[INFO]";
	static add_cptr<char> const warningPrefix	= "[WARNING]";
	static add_cptr<char> const errorPrefix		= "[ERROR]";
	static add_cptr<char> const debugPrefix		= "[DEBUG]";

	add_cptr<char> prefix = "[APP]";
	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		prefix = infoPrefix;
	else if (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
		prefix = warningPrefix;
	else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		prefix = errorPrefix;
	else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		prefix = debugPrefix;


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
