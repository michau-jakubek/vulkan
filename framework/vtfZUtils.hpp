#ifndef __VTF_Z_UTILS_HPP_INCLUDED__
#define __VTF_Z_UTILS_HPP_INCLUDED__

#include <cstddef>
#include <optional>
#include <ostream>
#include <type_traits>
#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#if SYSTEM_OS_LINUX == 1
  #include "demangle.hpp"
#endif

namespace vtf
{

class VertexInput;

#if SYSTEM_OS_LINUX == 1
template<class ZetDeletable,
		 typename std::enable_if< std::is_convertible< add_ptr<ZetDeletable>, add_ptr<ZDeletableMark> >::value, bool>::type = true>
std::ostream& operator<<(std::ostream& str, const ZetDeletable& z)
{
	str << demangledName<typename ZetDeletable::vulkan_type>()
		<< ": " << static_cast<void*>(*z);
	return str;
}
#endif // SYSTEM_OS_LINUX

Version			getVulkanImplVersion (std::optional<ZInstance> instance = {});

ZInstance		createInstance (const char*							appName,
								VkAllocationCallbacksPtr			callbacks,
								const strings&						desiredLayers = {},
								const strings&						desiredExtensions = {},
								add_ptr<VkDebugUtilsMessengerEXT>	pMessenger = nullptr,
								void*								pMessengerUserData = nullptr,
								add_ptr<VkDebugReportCallbackEXT>	pReport = nullptr,
								void*								pReportUserData = nullptr,
								uint32_t							apiVersion = VK_API_VERSION_1_0,
								bool								enableDebugPrintf = false);
ZPhysicalDevice	getPhysicalDeviceByIndex	(ZInstance									instance,
											 uint32_t									physicalDeviceIndex);
ZPhysicalDevice selectPhysicalDevice		(const uint32_t								proposedDeviceIndex,
											 ZInstance									instance,
											 const strings&								requiredExtensions,
											 std::map<VkQueueFlagBits, uint32_t>&		queuesToIndices,
											 std::optional<ZSurfaceKHR>					surface = {},
											 add_ptr<uint32_t>							pPresentQueueFamilyIndex = nullptr);
// If present then prepare deviceExtensions you want and return your VkPhysicalDeviceFeatures2 struct
// with desired features enabled. You will not care about sType field and pNext field of that struct
// is being returned will pass as VkDeviceCreateInfo::pNext during logical device creation.
typedef std::function<VkPhysicalDeviceFeatures2(ZPhysicalDevice, add_ref<strings> deviceExtensions)> GetEnabledFeaturesCB;
ZDevice			createLogicalDevice			(ZPhysicalDevice							physDevice,
											 const std::map<VkQueueFlagBits, uint32_t>&	queuesToIndices,
											 GetEnabledFeaturesCB						onGetEnabledFeatures = {},
											 const uint32_t								presentQueueFamilyIndex = INVALID_UINT32);


ZFence			createFence		(ZDevice device, bool signaled = false);
void			waitForFence	(ZFence fence, uint64_t timeout = UINT64_MAX);
void			resetFence		(ZFence fence);
void			waitForFences	(std::initializer_list<ZFence> fences, uint64_t timeout = UINT64_MAX);
void			resetFences		(std::initializer_list<ZFence> fences);
void			waitForFences	(std::vector<ZFence> fences, uint64_t timeout = UINT64_MAX);
void			resetFences		(std::vector<ZFence> fences);
ZSemaphore		createSemaphore	(ZDevice device);

ZShaderModule	createShaderModule(ZDevice device, const std::string& code);
ZShaderModule	createShaderModule(ZDevice device, const std::vector<unsigned char>& code);

ZPipeline		createGraphicsPipeline (ZPipelineLayout layout, ZRenderPass renderPass, VkExtent2D extent, ZShaderModule vertShaderModule, ZShaderModule fragShaderModule);

bool			pointInTriangle2D (const Vec2& p, const Vec2& p0, const Vec2& p1, const Vec2& p2, bool bar = false);
bool			pointInTriangle2D (const Vec3& p, const Vec3& p0, const Vec3& p1, const Vec3& p2, bool bar = false);
bool			pointInTriangle2D (const Vec4& p, const Vec4& p0, const Vec4& p1, const Vec4& p2, bool bar = false);

/*
template<class PixelType, class MapFun>
std::ostream& printImageLeveled (ZCommandPool commandPool, ZImage image, MapFun&& mapFun, std::ostream& str = std::cout)
{
	ZBuffer		buffer			= createBuffer(image);
	copyImageToBuffer(commandPool, image, buffer);
	auto		pixels			= readBuffer<PixelType>(buffer);
	const auto	ci				= viewportImage.getParam<VkImageCreateInfo>();

	int			levelOffset		= 0;
	int			width			= ci.extent.width;
	int			height			= ci.extent.height;
	const int	availableLevels = ci.mipLevels;

	str << "printImageLeveled::pixelCount: " << pixels.size() << std::endl;

	for (int level = 0; level < availableLevels; ++level)
	{
		str << "Level: " << level << " " << width << "x" << height << ' ' << " pixels: " << pixels.size() << std::endl;

		for (int y = height-1; y >= 0; --y)
		{
			for (int x = 0; x < width; ++x)
			{
				auto px = pixels[((y * width) + x) + levelOffset];
				str << mapFun(px) << ' ';
				//std::cout << px << ' ';
			}
			str << std::endl;
		}
		levelOffset += (width * height);
		width /= 2;
		height /= 2;
	}

	str << "Offset: " << levelOffset << std::endl;
	return str;
}
*/

} // namespace vtf

#endif // __VTF_Z_UTILS_HPP_INCLUDED__
