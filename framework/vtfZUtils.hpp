#ifndef __VTF_Z_UTILS_HPP_INCLUDED__
#define __VTF_Z_UTILS_HPP_INCLUDED__

#include <ostream>
#include <cstddef>
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

ZInstance		createInstance (const char*							appName,
								VkAllocationCallbacksPtr			callbacks,
								const strings&						requiredLayers = {},
								const strings&						requiredExtensions = {},
								add_ptr<VkDebugUtilsMessengerEXT>	pMessenger = nullptr,
								void*								pMessengerUserData = nullptr,
								uint32_t							apiVersion = VK_API_VERSION_1_0,
								uint32_t							engVersion = VK_MAKE_VERSION(1, 0, 0),
								uint32_t							appVersion = VK_MAKE_VERSION(1, 0, 0));
ZPhysicalDevice	getPhysicalDeviceByIndex	(ZInstance									instance,
											 uint32_t									physicalDeviceIndex);
ZPhysicalDevice selectPhysicalDevice		(const uint32_t								proposedDeviceIndex,
											 ZInstance									instance,
											 const strings&								requiredExtensions,
											 std::map<VkQueueFlagBits, uint32_t>&		queuesToIndices,
											 std::optional<ZSurfaceKHR>					surface = {},
											 add_ptr<uint32_t>							pPresentQueueFamilyIndex = nullptr);
ZDevice			createLogicalDevice			(ZPhysicalDevice							physDevice,
											 const std::map<VkQueueFlagBits, uint32_t>&	queuesToIndices,
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

VkSamplerCreateInfo makeSamplerCreateInfo	(VkSamplerAddressMode	wrapS		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkSamplerAddressMode	wrapT		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkFilter				minFilter	= VK_FILTER_NEAREST,
											 VkFilter				magFilter	= VK_FILTER_NEAREST,
											 bool					normalized	= true);
ZSampler			createSampler			(ZDevice device, const VkSamplerCreateInfo& samplerCreateInfo);

ZImage				createImage				(ZDevice device, VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
											 uint32_t mipLevels = 1, uint32_t layers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
											 VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
ZImageView			createImageView			(ZImage image, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, VkFormat chgfmt = VK_FORMAT_UNDEFINED,
											 uint32_t baseLevel = 0, uint32_t levels = 1, uint32_t baseLayer = 0, uint32_t layers = 1);


VkImageLayout	changeImageLayout (ZImage image, VkImageLayout layout);
VkImageLayout	changeImageLayout (ZImageView view, VkImageLayout layout);

uint32_t		computeBufferPixelCount (ZImage image, uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32);
uint32_t		computeBufferPixelCount (ZImageView image);
VkDeviceSize	computeBufferSize (ZImage image, uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32);
VkDeviceSize	computeBufferSize (ZImageView view);
void			transitionImage (ZCommandBuffer cmd, VkImage image, VkFormat format,
								 VkImageLayout			initialLayout,	VkImageLayout			targetLayout,
								 VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
								 VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage);
void			transitionImage (ZCommandBuffer cmd, ZImage image, VkImageLayout targetLayout,
								 VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
								 VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage);
void			copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImage image,
								   uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32,
								   VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL);
void			copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImageView view, VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL);
void			copyImageToBuffer (ZCommandPool commandPool, ZImage image, ZBuffer buffer, uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32);
void			copyImageToBuffer (ZCommandPool commandPool, ZImageView view, ZBuffer buffer);
void			copyImageToBuffer (ZCommandBuffer commandBuffer, ZImage image, ZBuffer buffer, uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32);
void			copyBufferToBuffer (ZCommandBuffer cmd, ZBuffer src, ZBuffer dst, const VkBufferCopy& region);
bool			pointInTriangle2D (const Vec2& p, const Vec2& p0, const Vec2& p1, const Vec2& p2);
bool			pointInTriangle2D (const Vec3& p, const Vec3& p0, const Vec3& p1, const Vec3& p2);

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
