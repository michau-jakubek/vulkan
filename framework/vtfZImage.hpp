#ifndef __VTF_ZIMAGE_UTILS_HPP_INCLUDED__
#define __VTF_ZIMAGE_UTILS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeviceMemory.hpp"

#include <chrono>
#include <ratio>

namespace vtf
{

VkSamplerCreateInfo makeSamplerCreateInfo	(VkSamplerAddressMode	wrapS		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkSamplerAddressMode	wrapT		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
											 VkFilter				minFilter	= VK_FILTER_NEAREST,
											 VkFilter				magFilter	= VK_FILTER_NEAREST,
											 bool					normalized	= true);
VkImageMemoryBarrier	makeImageMemoryBarrier	(ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED);
VkImageSubresourceRange	makeImageSubresourceRange	(ZImage image);
ZSampler			createSampler	(ZDevice device, const VkSamplerCreateInfo& samplerCreateInfo);
ZImage				createImage		(ZDevice device, VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
									 uint32_t mipLevels = 1, uint32_t layers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
									 VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
VkImageViewType		imageTypeToViewType (VkImageType imageType);
ZImageView			createImageView	(ZImage image,
									 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
									 VkFormat chgfmt = VK_FORMAT_UNDEFINED,
									 VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM,
									 uint32_t baseLevel = INVALID_UINT32, uint32_t levels = INVALID_UINT32,
									 uint32_t baseLayer = INVALID_UINT32, uint32_t layers = INVALID_UINT32);
void				transitionImage (ZCommandBuffer cmd, VkImage image, VkFormat format,
									 VkImageLayout			initialLayout,	VkImageLayout			targetLayout,
									 VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
									 VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage);
void				transitionImage (ZCommandBuffer cmd, ZImage image, VkImageLayout targetLayout,
									 VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
									 VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage);
void				copyImageToBuffer (ZCommandPool commandPool, ZImage image, ZBuffer buffer,
									   uint32_t baseLevel = 0u, uint32_t levels = 1u,
									   uint32_t baseLayer = 0u, uint32_t layers = 1u, bool zeroBuffer = false);
void				copyImageToBuffer (ZCommandPool commandPool, ZImageView view, ZBuffer buffer);
void				copyImageToBuffer (ZCommandBuffer commandBuffer, ZImage image, ZBuffer buffer,
									   uint32_t baseLevel = 0u, uint32_t levels = 1u,
									   uint32_t baseLayer = 0u, uint32_t layers = 1u, bool zeroBuffer = false);
VkImageLayout		changeImageLayout (ZImage image, VkImageLayout layout);
VkImageLayout		changeImageLayout (ZImageView view, VkImageLayout layout);

template<class Comp, size_t N>
auto				completeVector	  (const std::vector<VecX<Comp,N>>& src, std::initializer_list<Comp> missings) -> std::vector<VecX<Comp,4>>;

struct SizeX;
struct SizeY;
struct SizeZ;

struct AnySize
{
	struct U
	{
		virtual uint32_t i(uint32_t,uint32_t,uint32_t) const = 0;
		virtual ~U() = default;
	};
	template<class L> struct V : U
	{
		L l;
		V(L&& m) : l(m) {}
		virtual uint32_t i(uint32_t x,uint32_t y,uint32_t z) const override
		{
			return l(x,y,z);
		}
	};
	std::shared_ptr<U> p;
	AnySize() : p() {}
	template<class L> AnySize(L&& l) : p(new V<L>(std::forward<L>(l))) {}
	uint32_t get(uint32_t x,uint32_t y,uint32_t z) const
	{
		return p->i(x,y,z);
	}
	bool valid() const { return p != nullptr; }
};

bool	readImageFileMetadata		(const std::string& fileName, add_ref<VkFormat> format, add_ref<uint32_t> width, add_ref<uint32_t> height);
auto	loadImageFileContent		(const std::string& fileName) -> std::vector<Vec4>;
ZBuffer	createBufferFromAndLoadImageFile	(ZDevice device, const std::string& fileName, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);

struct ImageLoadHelper
{
	ZImage					m_srcImage;
	ZImage					m_dstImage;
	const std::string		m_fileName;
	std::optional<ZBuffer>	m_buffer;
	// valid after return from loadImageFromFile(...) function
	uint32_t				m_fileImageWidth;
	uint32_t				m_fileImageHeight;
	VkFormat				m_fileImageFormat;
	VkImageLayout			m_finalLayout;
	VkAccessFlags			m_finalAccess;
	VkPipelineStageFlags	m_finalPipelineStage;
	ImageLoadHelper (ZImage image, const std::string& imageFileName, bool loadBufferNow = false);
};
ZImage	createImageFromFileMetadata	(ZDevice device, const std::string& imageFileName,
									 ZImageUsageFlags usage = ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT),
									 const AnySize& desiredWidth = AnySize(), const AnySize& desiredHeight = AnySize());

ZImage	createImageAndLoadFromFile	(ZCommandPool commandPool, const std::string& fileName,
									 ZImageUsageFlags usage = ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT),
									 const AnySize& width = AnySize(), const AnySize& height = AnySize(),
									 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);

void	loadImageFromFile			(ImageLoadHelper& helper, ZCommandBuffer commandBuffer, VkImageLayout finalLayout,
									 VkAccessFlags srcAccess, VkAccessFlags dstAccess,
									 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

void	loadImageFromFile			(ZImage image, const std::string& fileName, ZCommandPool commandPool,
									 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL);

ZImage	createCubeImageAndLoadFromFiles	(ZDevice device, ZCommandPool commandPool, const strings& fileNames,
										 ZImageUsageFlags usage = ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT),
										 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
										 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL,
										 VkAccessFlags finalAccess = (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
} // namespace vtf

#endif // __VTF_ZIMAGE_UTILS_HPP_INCLUDED__
