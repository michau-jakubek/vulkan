#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfVulkanDriver.hpp"
#include "vulkan/vulkan_to_string.hpp"

#include <functional>
#include <inttypes.h>

#define MKN(enumConstant) #enumConstant
#define MKP(enumConstant) { enumConstant, MKN(enumConstant) }
#define MKPT(enumType, enumConstant, enumName) { enumType(enumConstant), enumName }
#define MK_CASE_RETURN_STRING(enumConstant) case enumConstant: return MKN(enumConstant)

namespace vtf
{

// Unfortunately not thread-safe
std::string vkResultToString (VkResult res)
{
	return vk::to_string(static_cast<vk::Result>(res));
}

std::ostream& operator<<(std::ostream& str, add_cref<ZDistType<QueueFlags, VkQueueFlags>> flags)
{
	return str << vk::to_string(static_cast<vk::QueueFlags>(flags.get()));
}

std::ostream& operator<< (std::ostream& str, add_cref<ZDeviceQueueCreateInfo> props)
{
	str << "VkQueueFamilyProperties[" << props.queueFamilyIndex << "] {\n"
		<< "\tqueueFlags:     " << ZDistType<QueueFlags, VkQueueFlags>(props.queueFlags) << '\n'
		<< "\tqueueCount:     " << props.queueCount << '\n'
		<< "\tsurfaceSupport: " << std::boolalpha << props.surfaceSupport << std::endl;
	return str;
}

std::ostream& operator<< (std::ostream& str, add_cref<VkPrimitiveTopology> topo)
{
	return str << vk::to_string(static_cast<vk::PrimitiveTopology>(topo));
}

std::ostream& operator<< (std::ostream& str, add_cref<VkShaderStageFlagBits> stage)
{
	return str << vk::to_string(static_cast<vk::ShaderStageFlagBits>(stage));
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
		VKASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, index, surfaceKHR, &presentSupport));
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
			VKASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, index, *surface, &presentSupport));
			if (presentSupport) indices.emplace_back(index);
		}
	}
	return indices;
}

bool hasFormatsAndModes (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR)
{
	uint32_t formatCount		= 0;
	uint32_t presentModeCount	= 0;
	VKASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surfaceKHR, &formatCount, nullptr));
	VKASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surfaceKHR, &presentModeCount, nullptr));
	return (formatCount != 0) && (presentModeCount != 0);
}

strings enumerateInstanceLayers ()
{
	strings propNames;
	uint32_t propertyCount = 0;

	const PFN_vkEnumerateInstanceLayerProperties proc =
		getDriverEnumerateInstanceLayerPropertiesProcAddr();
	ASSERTMSG(proc, "Unable to get vkEnumerateInstanceLayerProperties()");
	VKASSERT((*proc)(&propertyCount, nullptr));

	if (propertyCount)
	{
		propNames.resize(propertyCount);
		std::vector<VkLayerProperties> props(propertyCount);
		VKASSERT((*proc)(&propertyCount, props.data()));
		std::transform(props.begin(), props.end(), propNames.begin(),
					   [](VkLayerProperties& p){return std::string(p.layerName);});
	}

	return propNames;
}

strings enumerateInstanceExtensions (const char* layerName)
{
	strings extensions;
	uint32_t extensionCount = 0;
	const PFN_vkEnumerateInstanceExtensionProperties proc =
		getDriverEnumerateInstanceExtensionPropertiesProcAddr();
	ASSERTMSG((proc != nullptr), "Unable to get vkEnumerateInstanceExtensionProperties()");
	VKASSERTMSG((*proc)(layerName, &extensionCount, nullptr),
		"pLayerName = ", (layerName ? layerName : "nullptr"), ", pPropertyCount = 0");

	if (extensionCount)
	{
		extensions.resize(extensionCount);
		std::vector<VkExtensionProperties> props(extensionCount);
		VKASSERTMSG((*proc)(layerName, &extensionCount, props.data()),
			"pLayerName = ", (layerName ? layerName : "nullptr"), ", pPropertyCount = ", extensionCount);
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
		mergeStringsDistinct(extensions, enumerateInstanceExtensions(layerName.c_str()));
	}
	return extensions;
}

static strings enumerateDeviceExtensions (VkPhysicalDevice device, const char* layerName)
{
	strings		extensions;
	uint32_t	extensionCount = 0;

	VKASSERT(vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, nullptr));

	if (extensionCount)
	{
		extensions.resize(extensionCount);
		std::vector<VkExtensionProperties> props(extensionCount);
		VKASSERT(vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, props.data()));
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
		mergeStringsDistinct(extensions, enumerateDeviceExtensions(device, layerName.c_str()));
	}
	return extensions;
}

uint32_t enumeratePhysicalDevices (VkInstance instance, std::vector<VkPhysicalDevice>& devices)
{
	uint32_t deviceCount = 0;
	VKASSERT(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
	devices.resize(deviceCount, VkPhysicalDevice(0));
	VKASSERT(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));
	return deviceCount;
}

uint32_t enumerateSwapchainImages (VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images)
{
	uint32_t imageCount = 0;
	VKASSERT(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
	images.resize(imageCount);
	VKASSERT(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));
	return imageCount;
}

add_ref<std::ostream> printPhysicalDeviceFeatures (
	add_cref<VkPhysicalDeviceFeatures> features,
	add_ref<std::ostream> str,
	uint32_t indent)
{
	const std::string si(indent, ' ');
	if (features.robustBufferAccess)						str << si << "robustBufferAccess: true" << std::endl;
	if (features.fullDrawIndexUint32)						str << si << "fullDrawIndexUint32: true" << std::endl;
	if (features.imageCubeArray)							str << si << "imageCubeArray: true" << std::endl;
	if (features.independentBlend)							str << si << "independentBlend: true" << std::endl;
	if (features.geometryShader)							str << si << "geometryShader: true" << std::endl;
	if (features.tessellationShader)						str << si << "tessellationShader: true" << std::endl;
	if (features.sampleRateShading)							str << si << "sampleRateShading: true" << std::endl;
	if (features.dualSrcBlend)								str << si << "dualSrcBlend: true" << std::endl;
	if (features.logicOp)									str << si << "logicOp: true" << std::endl;
	if (features.multiDrawIndirect)							str << si << "multiDrawIndirect: true" << std::endl;
	if (features.drawIndirectFirstInstance)					str << si << "drawIndirectFirstInstance: true" << std::endl;
	if (features.depthClamp)								str << si << "depthClamp: true" << std::endl;
	if (features.depthBiasClamp)							str << si << "depthBiasClamp: true" << std::endl;
	if (features.fillModeNonSolid)							str << si << "fillModeNonSolid: true" << std::endl;
	if (features.depthBounds)								str << si << "depthBounds: true" << std::endl;
	if (features.wideLines)									str << si << "wideLines: true" << std::endl;
	if (features.largePoints)								str << si << "largePoints: true" << std::endl;
	if (features.alphaToOne)								str << si << "alphaToOne: true" << std::endl;
	if (features.multiViewport)								str << si << "multiViewport: true" << std::endl;
	if (features.samplerAnisotropy)							str << si << "samplerAnisotropy: true" << std::endl;
	if (features.textureCompressionETC2)					str << si << "textureCompressionETC2: true" << std::endl;
	if (features.textureCompressionASTC_LDR)				str << si << "textureCompressionASTC_LDR: true" << std::endl;
	if (features.textureCompressionBC)						str << si << "textureCompressionBC: true" << std::endl;
	if (features.occlusionQueryPrecise)						str << si << "occlusionQueryPrecise: true" << std::endl;
	if (features.pipelineStatisticsQuery)					str << si << "pipelineStatisticsQuery: true" << std::endl;
	if (features.vertexPipelineStoresAndAtomics)			str << si << "vertexPipelineStoresAndAtomics: true" << std::endl;
	if (features.fragmentStoresAndAtomics)					str << si << "fragmentStoresAndAtomics: true" << std::endl;
	if (features.shaderTessellationAndGeometryPointSize)	str << si << "shaderTessellationAndGeometryPointSize: true" << std::endl;
	if (features.shaderImageGatherExtended)					str << si << "shaderImageGatherExtended: true" << std::endl;
	if (features.shaderStorageImageExtendedFormats)			str << si << "shaderStorageImageExtendedFormats: true" << std::endl;
	if (features.shaderStorageImageMultisample)				str << si << "shaderStorageImageMultisample: true" << std::endl;
	if (features.shaderStorageImageReadWithoutFormat)		str << si << "shaderStorageImageReadWithoutFormat: true" << std::endl;
	if (features.shaderStorageImageWriteWithoutFormat)		str << si << "shaderStorageImageWriteWithoutFormat: true" << std::endl;
	if (features.shaderUniformBufferArrayDynamicIndexing)	str << si << "shaderUniformBufferArrayDynamicIndexing: true" << std::endl;
	if (features.shaderSampledImageArrayDynamicIndexing)	str << si << "shaderSampledImageArrayDynamicIndexing: true" << std::endl;
	if (features.shaderStorageBufferArrayDynamicIndexing)	str << si << "shaderStorageBufferArrayDynamicIndexing: true" << std::endl;
	if (features.shaderStorageImageArrayDynamicIndexing)	str << si << "shaderStorageImageArrayDynamicIndexing: true" << std::endl;
	if (features.shaderClipDistance)						str << si << "shaderClipDistance: true" << std::endl;
	if (features.shaderCullDistance)						str << si << "shaderCullDistance: true" << std::endl;
	if (features.shaderFloat64)								str << si << "shaderFloat64: true" << std::endl;
	if (features.shaderInt64)								str << si << "shaderInt64: true" << std::endl;
	if (features.shaderInt16)								str << si << "shaderInt16: true" << std::endl;
	if (features.shaderResourceResidency)					str << si << "shaderResourceResidency: true" << std::endl;
	if (features.shaderResourceMinLod)						str << si << "shaderResourceMinLod: true" << std::endl;
	if (features.sparseBinding)								str << si << "sparseBinding: true" << std::endl;
	if (features.sparseResidencyBuffer)						str << si << "sparseResidencyBuffer: true" << std::endl;
	if (features.sparseResidencyImage2D)					str << si << "sparseResidencyImage2D: true" << std::endl;
	if (features.sparseResidencyImage3D)					str << si << "sparseResidencyImage3D: true" << std::endl;
	if (features.sparseResidency2Samples)					str << si << "sparseResidency2Samples: true" << std::endl;
	if (features.sparseResidency4Samples)					str << si << "sparseResidency4Samples: true" << std::endl;
	if (features.sparseResidency8Samples)					str << si << "sparseResidency8Samples: true" << std::endl;
	if (features.sparseResidency16Samples)					str << si << "sparseResidency16Samples: true" << std::endl;
	if (features.sparseResidencyAliased)					str << si << "sparseResidencyAliased: true" << std::endl;
	if (features.variableMultisampleRate)					str << si << "variableMultisampleRate: true" << std::endl;
	if (features.inheritedQueries)							str << si << "inheritedQueries: true" << std::endl;
	return str;
}

std::ostream& printPhysicalDevice (
    add_cref<VkPhysicalDeviceProperties> props,
    std::ostream&                        str,
    uint32_t                             deviceIndex)
{
    const Version apiVersion = Version::fromUint(props.apiVersion);
    const uint32_t driverMajorVersion = (props.driverVersion >> 22) & 0x7F;
    const uint32_t driverMinorVersion = (props.driverVersion >> 12) & 0x3FF;
    const uint32_t driverPatchVersion = props.driverVersion & 0xFFF;

    if (deviceIndex != INVALID_UINT32)
    {
        str << deviceIndex << ") ";
    }
    str << "Name: \"" << props.deviceName
        << "\"  Vendor: " << std::hex << props.vendorID
        << "  Device: " << std::hex << props.deviceID << std::endl;
    str << std::dec << "  " << " API: " << apiVersion;
    str << ", Driver version: (" << driverMajorVersion << ", "
        << driverMinorVersion << ", " << driverPatchVersion << ')' << std::endl;
    return str;
}

std::ostream& printPhysicalDevice (
    VkPhysicalDevice    device,
    std::ostream&       str,
    uint32_t            deviceIndex)
{
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(device, &p);
    printPhysicalDevice(p, str, deviceIndex);
    return str;
}

std::ostream& printPhysicalDevices (
    VkInstance      instance,
    std::ostream&   str)
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
            printPhysicalDevice(devices[i], str, i);
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

VkExtent2D makeExtent2D (add_cref<VkExtent3D> extent3D)
{
	return { extent3D.width, extent3D.height };
}

VkRect2D makeRect2D (uint32_t width, uint32_t height, int32_t Xoffset, int32_t Yoffset)
{
	VkRect2D rect;
	rect.extent = { width, height };
	rect.offset = { Xoffset, Yoffset };
	return rect;
}

VkRect2D makeRect2D (add_cref<VkExtent2D> extent2D, add_cref<VkOffset2D> offset2D)
{
	VkRect2D rect;
	rect.extent	= extent2D;
	rect.offset	= offset2D;
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

VkOffset2D makeOffset2D (int32_t x, int32_t y)
{
	VkOffset2D offset{};
	offset.x = x;
	offset.y = y;
	return offset;
}

VkOffset3D makeOffset3D (int32_t x, int32_t y, int32_t z)
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

VkViewport makeViewport (add_cref<VkExtent2D> extent, float minDepth, float maxDepth)
{
	VkViewport port;
	port.width = static_cast<float>(extent.width);
	port.height = static_cast<float>(extent.height);
	port.x = 0.0f;
	port.y = 0.0f;
	port.minDepth = minDepth;
	port.maxDepth = maxDepth;
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

VkColorBlendEquationEXT makeColorBlendEquationExt(add_cref<VkPipelineColorBlendAttachmentState> state)
{
	VkColorBlendEquationEXT eq{};
	eq.srcColorBlendFactor	= state.srcColorBlendFactor;
	eq.dstColorBlendFactor	= state.dstColorBlendFactor;
	eq.colorBlendOp			= state.colorBlendOp;
	eq.srcAlphaBlendFactor	= state.srcAlphaBlendFactor;
	eq.dstAlphaBlendFactor	= state.dstAlphaBlendFactor;
	eq.alphaBlendOp = state.alphaBlendOp;
	return eq;
}

} // namespace vtf
