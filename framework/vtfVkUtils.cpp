#include <functional>
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"

#define MKP(enumConstant) { enumConstant, #enumConstant }

namespace vtf
{

const char* vkResultToString (VkResult res)
{
	struct { VkResult v; const char* s; }
	const results[]
	{
		MKP(VK_SUCCESS),
		MKP(VK_NOT_READY),
		MKP(VK_TIMEOUT),
		MKP(VK_EVENT_SET),
		MKP(VK_EVENT_RESET),
		MKP(VK_INCOMPLETE),
		MKP(VK_ERROR_OUT_OF_HOST_MEMORY),
		MKP(VK_ERROR_OUT_OF_DEVICE_MEMORY),
		MKP(VK_ERROR_INITIALIZATION_FAILED),
		MKP(VK_ERROR_DEVICE_LOST),
		MKP(VK_ERROR_MEMORY_MAP_FAILED),
		MKP(VK_ERROR_LAYER_NOT_PRESENT),
		MKP(VK_ERROR_EXTENSION_NOT_PRESENT),
		MKP(VK_ERROR_FEATURE_NOT_PRESENT),
		MKP(VK_ERROR_INCOMPATIBLE_DRIVER),
		MKP(VK_ERROR_TOO_MANY_OBJECTS),
		MKP(VK_ERROR_FORMAT_NOT_SUPPORTED),
		MKP(VK_ERROR_FRAGMENTED_POOL),
		MKP(VK_ERROR_UNKNOWN),
		MKP(VK_ERROR_OUT_OF_POOL_MEMORY),
		MKP(VK_ERROR_INVALID_EXTERNAL_HANDLE),
		MKP(VK_ERROR_FRAGMENTATION),
		MKP(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS),
		MKP(VK_PIPELINE_COMPILE_REQUIRED),
		MKP(VK_ERROR_SURFACE_LOST_KHR),
		MKP(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR),
		MKP(VK_SUBOPTIMAL_KHR),
		MKP(VK_ERROR_OUT_OF_DATE_KHR),
		MKP(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR),
		MKP(VK_ERROR_VALIDATION_FAILED_EXT),
		MKP(VK_ERROR_INVALID_SHADER_NV),
		MKP(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT),
		MKP(VK_ERROR_NOT_PERMITTED_KHR),
		MKP(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT),
		MKP(VK_THREAD_IDLE_KHR),
		MKP(VK_THREAD_DONE_KHR),
		MKP(VK_OPERATION_DEFERRED_KHR),
		MKP(VK_OPERATION_NOT_DEFERRED_KHR),
		MKP(VK_ERROR_OUT_OF_POOL_MEMORY_KHR),				// = VK_ERROR_OUT_OF_POOL_MEMORY,
		MKP(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR),			// = VK_ERROR_INVALID_EXTERNAL_HANDLE,
		MKP(VK_ERROR_FRAGMENTATION_EXT),					// = VK_ERROR_FRAGMENTATION,
		MKP(VK_ERROR_NOT_PERMITTED_EXT),					// = VK_ERROR_NOT_PERMITTED_KHR,
		MKP(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT),			// = VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,
		MKP(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR),	// = VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,
		MKP(VK_PIPELINE_COMPILE_REQUIRED_EXT),				// = VK_PIPELINE_COMPILE_REQUIRED,
		MKP(VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT),		// = VK_PIPELINE_COMPILE_REQUIRED,
	};
	for (auto& r : results)
	{
		if (r.v == res) return r.s;
	}
	return "<VkResult UNDEFINED ERROR>";
}

template<class X, class... Y> X emplace (Y&&... y)
{
	return x(std::forward<Y>(y)...);
}

template<class X, class Y, class Z> X construct(Z Y::* z)
{
	return std::bind(emplace<X,Z>, std::mem_fn(z), std::placeholders::_1);
}

uint32_t findQueueFamilyIndex (VkPhysicalDevice phDevice, VkQueueFlagBits bit)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(phDevice, &queueFamilyCount, queueFamilies.data());

	for (uint32_t index = 0; index < queueFamilyCount; ++index)
	{
		if (bit & queueFamilies[index].queueFlags)
			return index;
	}

	return INVALID_UINT32;
}

uint32_t findSurfaceSupportedQueueFamilyIndex (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
	for (uint32_t index = 0; index < queueFamilyCount; ++index)
	{
		VkBool32 presentSupport = false;
		VKASSERT2(vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, index, surfaceKHR, &presentSupport));
		if (presentSupport) return index;
	}
	return INVALID_UINT32;
}

bool hasFormatsAndModes (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR)
{
	uint32_t formatCount		= 0;
	uint32_t presentModeCount	= 0;
	VKASSERT2(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surfaceKHR, &formatCount, nullptr));
	VKASSERT2(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surfaceKHR, &presentModeCount, nullptr));
	return (formatCount != 0) && (presentModeCount != 0);
}

strings enumerateInstanceLayers ()
{
	strings propNames;
	uint32_t propertyCount = 0;

	VKASSERT2(vkEnumerateInstanceLayerProperties(&propertyCount, nullptr));

	if (propertyCount)
	{
		propNames.resize(propertyCount);
		std::vector<VkLayerProperties> props(propertyCount);
		VKASSERT2(vkEnumerateInstanceLayerProperties(&propertyCount, props.data()));
		std::transform(props.begin(), props.end(), propNames.begin(),
					   [](VkLayerProperties& p){return std::string(p.layerName);});
	}

	return propNames;
}

static strings enumerateInstanceExtensions (const char* layerName)
{
	strings extensions;
	uint32_t extensionCount = 0;

	VKASSERT2(vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr));

	if (extensionCount)
	{
		extensions.resize(extensionCount);
		std::vector<VkExtensionProperties> props(extensionCount);
		VKASSERT2(vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, props.data()));
		std::transform(props.begin(), props.end(), extensions.begin(),
					   [](VkExtensionProperties& p){return std::string(p.extensionName);});
	}

	return extensions;
}

strings enumerateInstanceExtensions (const strings& layerNames)
{
	strings extensions	= enumerateInstanceExtensions(nullptr);
	for (const auto& layerName : layerNames)
	{
		extensions = mergeStringsDistinct(extensions, enumerateInstanceExtensions(layerName.c_str()));
	}
	return extensions;
}

static strings enumerateDeviceExtensions (VkPhysicalDevice device, const char* layerName)
{
	strings		extensions;
	uint32_t	extensionCount = 0;

	VKASSERT2(vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, nullptr));

	if (extensionCount)
	{
		extensions.resize(extensionCount);
		std::vector<VkExtensionProperties> props(extensionCount);
		VKASSERT2(vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, props.data()));
		std::transform(props.begin(), props.end(), extensions.begin(),
					   [](VkExtensionProperties& p){return std::string(p.extensionName);});
	}

	return extensions;
}

strings enumerateDeviceExtensions (VkPhysicalDevice device, const strings& layerNames)
{
	strings extensions = enumerateDeviceExtensions(device, nullptr);
	for (const auto& layerName : layerNames)
	{
		extensions = mergeStringsDistinct(extensions, enumerateDeviceExtensions(device, layerName.c_str()));
	}
	return extensions;
}

uint32_t enumeratePhysicalDevices (VkInstance instance, std::vector<VkPhysicalDevice>& devices)
{
	uint32_t deviceCount = 0;
	VKASSERT2(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
	devices.resize(deviceCount, VkPhysicalDevice(0));
	VKASSERT2(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));
	return deviceCount;
}

uint32_t enumerateSwapchainImages (VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images)
{
	uint32_t imageCount = 0;
	VKASSERT2(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
	images.resize(imageCount);
	VKASSERT2(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));
	return imageCount;
}

std::ostream& printPhysicalDevices (VkInstance instance, std::ostream& str)
{
	std::vector<VkPhysicalDevice> devices;
	enumeratePhysicalDevices(instance, devices);

	const uint32_t deviceCount = static_cast<uint32_t>(devices.size());

	if (deviceCount == 0) {
		str << "Unable to find any device with Vulkan support!" << std::endl;
	}
	else {
		str << "Found " << deviceCount << " physicalDevices" << std::endl;
		for (uint32_t i = 0; i < deviceCount; ++i)
		{
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(devices[i], &p);

			str << i << ") Name: \"" << p.deviceName << "\"  Vendor: " << std::hex << p.vendorID
				  << "  Device: " << std::hex << p.deviceID << std::endl;
			str << std::dec << "  " << " API: " << Version(p.apiVersion) << ", Driver version: " << Version(p.driverVersion) << std::endl;
		}
	}
	return str;
}

} // namespace vtf
