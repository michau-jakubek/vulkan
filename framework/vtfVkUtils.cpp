#include <functional>
#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfFormatUtils.hpp"

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

static const char* queueFlagBitsToString (VkQueueFlagBits bits)
{
	struct { VkQueueFlagBits b; const char* s; }
	const results[] {
		MKP(VK_QUEUE_GRAPHICS_BIT),
		MKP(VK_QUEUE_COMPUTE_BIT),
		MKP(VK_QUEUE_TRANSFER_BIT),
		MKP(VK_QUEUE_SPARSE_BINDING_BIT),
		MKP(VK_QUEUE_PROTECTED_BIT)
	};
	for (auto& r : results)
	{
		if (r.b == bits) return r.s;
	}
	return "<VkQueueFlagBits UNKNOWN>";
}

std::ostream& operator<<(std::ostream& str, add_cref<ZDistType<QueueFlags, VkQueueFlags>> flags)
{
	const VkQueueFlagBits bits[] {
		(VK_QUEUE_GRAPHICS_BIT),
		(VK_QUEUE_COMPUTE_BIT),
		(VK_QUEUE_TRANSFER_BIT),
		(VK_QUEUE_SPARSE_BINDING_BIT),
		(VK_QUEUE_PROTECTED_BIT)
	};
	for (uint32_t i = 0, j = 0; i < ARRAY_LENGTH(bits); ++i)
	{
		if ((bits[i] & flags) == make_unsigned(bits[i]))
		{
			if (j++) str << " | ";
			str << queueFlagBitsToString(bits[i]);
		}
	}
	return str;
}

std::ostream& operator<<(std::ostream& str, add_cref<VkDeviceQueueCreateInfoEx> props)
{
	str << "VkQueueFamilyProperties[" << props.queueFamilyIndex << "] {\n"
		<< "\tqueueFlags:     " << ZDistType<QueueFlags, VkQueueFlags>(props.queueFlags) << '\n'
		<< "\tqueueCount:     " << props.queueCount << '\n'
		<< "\tsurfaceSupport: " << std::boolalpha << props.surfaceSupport << std::endl;
	return str;
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

std::vector<uint32_t> findSurfaceSupportedQueueFamilyIndices (VkPhysicalDevice physDevice, ZSurfaceKHR surface)
{
	std::vector<uint32_t> indices;
	if (surface.has_handle())
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
		for (uint32_t index = 0; index < queueFamilyCount; ++index)
		{
			VkBool32 presentSupport = false;
			VKASSERT2(vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, index, *surface, &presentSupport));
			if (presentSupport) indices.emplace_back(index);
		}
	}
	return indices;
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

			Version apiVersion = Version::fromUint(p.apiVersion);
			Version driverVersion = Version::fromUint(p.driverVersion);

			str << i << ") Name: \"" << p.deviceName << "\"  Vendor: " << std::hex << p.vendorID
				  << "  Device: " << std::hex << p.deviceID << std::endl;
			str << std::dec << "  " << " API: " << apiVersion << ", Driver version: " << driverVersion << std::endl;
		}
	}
	return str;
}

uint32_t computeMipLevelCount (uint32_t width, uint32_t height)
{
	if (width == 0u || height == 0) return 0u;
	return static_cast<uint32_t>(std::min(std::floor(std::log2(width)), std::floor(std::log2(height)))) + 1;
}

std::pair<uint32_t,uint32_t> computeMipLevelWidthAndHeight (uint32_t width, uint32_t height, uint32_t level)
{
	uint32_t nextLevel = 0;
	while (width >= 1 && height >= 1 && nextLevel++ < level)
	{
		nextLevel += 1;
		height /= 2;
		width /= 2;
	}
	return { width, height };
}

VkDeviceSize computeMipLevelsPixelCount (uint32_t width, uint32_t height, uint32_t levelCount)
{
	ASSERTION(width >= 1 && height >= 1);
	VkDeviceSize count = 0;
	auto iLevelCount = make_signed(levelCount);
	while (width >= 1 && height >= 1 && --iLevelCount >= 0)
	{
		count += width * height;
		height /= 2;
		width /= 2;
	}
	return count;
}

std::pair<VkDeviceSize, VkDeviceSize>
computeMipLevelsOffsetAndSize (VkFormat format, uint32_t level0Width, uint32_t level0Height,
							   uint32_t baseMipLevel, uint32_t mipLevelCount)
{
	const uint32_t		pixelWidth	= formatGetInfo(format).pixelByteSize;
	const VkDeviceSize	offset		= computeMipLevelsPixelCount(level0Width, level0Height, baseMipLevel) * pixelWidth;
	std::tie(level0Width,level0Height) = computeMipLevelWidthAndHeight(level0Width, level0Height, baseMipLevel);
	const VkDeviceSize	size		= computeMipLevelsPixelCount(level0Width, level0Height, mipLevelCount) * pixelWidth;
	return { offset, size };
}

uint32_t computePixelByteSize (VkFormat format)
{
	const ZFormatInfo info = formatGetInfo(format);
	return info.pixelByteSize;
}

uint32_t computePixelChannelCount (VkFormat format)
{
	const ZFormatInfo info = formatGetInfo(format);
	return info.componentCount;
}

uint32_t sampleFlagsToSampleCount (VkSampleCountFlags flags)
{
	struct
	{
		VkSampleCountFlagBits	bits;
		uint32_t				mask;
	}
	inf[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT,	0x00000001 },
		{ VK_SAMPLE_COUNT_2_BIT,	0x00000002 },
		{ VK_SAMPLE_COUNT_4_BIT,	0x00000004 },
		{ VK_SAMPLE_COUNT_8_BIT,	0x00000008 },
		{ VK_SAMPLE_COUNT_16_BIT,	0x00000010 },
		{ VK_SAMPLE_COUNT_32_BIT,	0x00000020 },
		{ VK_SAMPLE_COUNT_64_BIT,	0x00000040 },
	};
	uint32_t count = 0;
	for (auto i : inf)
	{
		if (flags & i.bits)
			count |= i.mask;
	}
	ASSERTION(count > 0);
	return count;
}

VkExtent2D makeExtent2D (uint32_t width, uint32_t height)
{
	return { width, height };
}

VkRect2D makeRect2D (uint32_t width, uint32_t height, int32_t Xoffset, int32_t Yoffset)
{
	VkRect2D rect;
	rect.extent = { width, height };
	rect.offset = { Xoffset, Yoffset };
	return rect;
}

VkExtent3D makeExtent3D (uint32_t width, uint32_t height, uint32_t depth)
{
	VkExtent3D extent{};
	extent.width	= width;
	extent.height	= height;
	extent.depth	= depth;
	return extent;
}

VkOffset3D makeOffset3D (uint32_t x, uint32_t y, uint32_t z)
{
	VkOffset3D offset{};
	offset.x = x;
	offset.y = y;
	offset.z = z;
	return offset;
}

VkViewport makeViewport (uint32_t width, uint32_t height, uint32_t x, uint32_t y, float minDepth, float maxDepth)
{
	VkViewport port;
	port.width		= static_cast<float>(width);
	port.height		= static_cast<float>(height);
	port.x			= static_cast<float>(x);
	port.y			= static_cast<float>(y);
	port.minDepth	= minDepth;
	port.maxDepth	= maxDepth;
	return port;
}

VkRect2D clampScissorToViewport (add_cref<VkViewport> viewport, add_ref<VkRect2D> inOutScissor)
//(VkViewport viewport, VkRect2D inOutScissor)
{
	if (static_cast<decltype(viewport.x)>(inOutScissor.offset.x) < viewport.x)
		inOutScissor.offset.x = static_cast<decltype(inOutScissor.offset.x)>(viewport.x);

	if (static_cast<decltype(viewport.y)>(inOutScissor.offset.y) < viewport.y)
		inOutScissor.offset.y = static_cast<decltype(inOutScissor.offset.y)>(viewport.y);

//	if ((inOutScissor.offset.x + make_signed(inOutScissor.extent.width)) > viewport.width)
//		inOutScissor.extent.width = viewport.width - inOutScissor.extent.width;
//	if ((inOutScissor.offset.y + make_signed(inOutScissor.extent.height)) > viewport.height)
//		inOutScissor.extent.height = viewport.height - inOutScissor.extent.height;
	return inOutScissor;
}

// TODO add from table
// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#synchronization-access-types

template<> VkClearColorValue makeClearColorValue<uint32_t> (add_cref<UVec4> color)
{
	VkClearColorValue c;
	c.uint32[0] = color[0];
	c.uint32[1] = color[1];
	c.uint32[2] = color[2];
	c.uint32[3] = color[3];
	return c;
}

template<> VkClearColorValue makeClearColorValue<int32_t> (add_cref<IVec4> color)
{
	VkClearColorValue c;
	c.int32[0] = color[0];
	c.int32[1] = color[1];
	c.int32[2] = color[2];
	c.int32[3] = color[3];
	return c;
}

template<> VkClearColorValue makeClearColorValue<float> (add_cref<Vec4> color)
{
	VkClearColorValue c;
	c.float32[0] = color[0];
	c.float32[1] = color[1];
	c.float32[2] = color[2];
	c.float32[3] = color[3];
	return c;
}

} // namespace vtf
