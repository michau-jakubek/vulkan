#ifndef __VTF_ZIMAGE_UTILS_HPP_INCLUDED__
#define __VTF_ZIMAGE_UTILS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZBarriers.hpp"
#include "vtfZBarriers2.hpp"

#include <chrono>
#include <ratio>

namespace vtf
{

VkSamplerCreateInfo makeSamplerCreateInfo	(VkSamplerAddressMode	wrapS		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkSamplerAddressMode	wrapT		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkFilter				minFilter	= VK_FILTER_NEAREST,
											 VkFilter				magFilter	= VK_FILTER_NEAREST,
											 bool					normalized	= true);
ZSampler			createSampler	(ZDevice device,
									 VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t mipLevels = 1u,
									 bool filterLinearORnearest = true, bool normalized = true,
									 bool mipMapEnable = false, bool anisotropyEnable = false);
ZSampler			createSampler	(ZImageView view, bool filterLinearORnearest = true,
									 bool normalized = true, bool mipMapEnable = false, bool anisotropyEnable = false);
ZImage				createImage		(ZDevice device, VkFormat format, VkImageType type, uint32_t width, uint32_t height,
									 ZImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
									 uint32_t mipLevels = 1, uint32_t layers = 1, uint32_t depth = 1,
									 ZMemoryPropertyFlags properties = ZMemoryPropertyDeviceFlags);

VkImageViewType		imageTypeToViewType (VkImageType imageType);
ZImageView			createImageView	(ZImage image,
									 uint32_t baseMipLevel = 0u, uint32_t mipLevels = 1u,
									 uint32_t baseArrayLayer = 0u, uint32_t arrayLayers = 1u,
									 VkFormat format = VK_FORMAT_UNDEFINED, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM,
									 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM);
ZImage				imageViewGetImage (ZImageView view);
VkImageLayout		imageGetLayout	 (ZImage image);
VkImageLayout		imageResetLayout (ZImage image, VkImageLayout layout);
VkImageLayout		imageResetLayout (ZImageView view, VkImageLayout layout);
auto				imageGetCreateInfo (add_cref<ZImage> image) -> add_cref<VkImageCreateInfo>;
add_cref<VkExtent3D> imageGetExtent (add_cref<ZImage> image);
VkFormat			imageGetFormat	(ZImage image);
uint32_t			imageGetPixelWidth (ZImage image);
std::pair<VkDeviceSize, VkDeviceSize>
					imageGetMipLevelsOffsetAndSize	(ZImage image, uint32_t baseLevel = 0, uint32_t levelCount = 1);
VkDeviceSize		imageCalcMipLevelsSize (ZImage image, uint32_t baseMipLevel, uint32_t mipLevelCount);

uint32_t			imageComputePixelCount		(ZImage image);
VkImageSubresourceRange	imageMakeSubresourceRange (ZImage image,
												   uint32_t baseMipLevel = 0u, uint32_t levelCount = 1u,
												   uint32_t baseArrayLayer = 0u, uint32_t arrayLayers = 1u);
VkImageSubresourceLayers imageMakeSubresourceLayers (ZImage image, uint32_t mipLevel = 0u,
													 uint32_t baseArrayLayer = 0u, uint32_t layerCount = 1u);
bool	readImageFileMetadata		(add_cref<std::string> fileName, add_ref<VkFormat> format, add_ref<uint32_t> width, add_ref<uint32_t> height);
ZImage	createImageFromFileMetadata	(ZDevice device, add_cref<std::string> imageFileName,
									 ZImageUsageFlags usage = ZImageUsageFlags(),
									 ZMemoryPropertyFlags memory = ZMemoryPropertyDeviceFlags,
									 bool forceFourComponentFormat = true);
ZImage	createImageFromFileMetadata	(ZDevice device, ZBuffer imageContentBuffer,
									 ZImageUsageFlags usage = ZImageUsageFlags(),
									 ZMemoryPropertyFlags memory = ZMemoryPropertyDeviceFlags);

ZImage	createCubeImageAndLoadFromFiles	(ZDevice device, ZCommandPool commandPool, const strings& fileNames,
										 ZImageUsageFlags usage = ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT),
										 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
										 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL,
										 VkAccessFlags finalAccess = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));

struct ZNonDeletableImage : public ZImage
{
	ZNonDeletableImage ();
	ZNonDeletableImage (VkImage, add_cref<ZDevice>, add_cref<VkImageCreateInfo>, add_cref<VkDeviceSize>);
	static ZNonDeletableImage create (VkImage image, ZDevice device,
									  VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
									  uint32_t mipLevels = 1, uint32_t layers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
};

} // namespace vtf

#endif // __VTF_ZIMAGE_UTILS_HPP_INCLUDED__
