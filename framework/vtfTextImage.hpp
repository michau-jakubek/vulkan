#ifndef __VTF_TEXT_IMAGE_HPP_INCLUDED__
#define __VTF_TEXT_IMAGE_HPP_INCLUDED__

#include "vtfZCommandBuffer.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZImage.hpp"

namespace vtf
{
ZBuffer createTextImageFontBuffer (ZDevice device);

ZImage createTextImage (ZCommandPool			cmdPool,
						add_cref<std::string>	text,
						add_cref<VkExtent2D>	imageRect,
						add_cref<VkExtent2D>	charRect,
						const VkFormat			format = VK_FORMAT_R32G32B32A32_SFLOAT,
						const VkImageLayout		layout = VK_IMAGE_LAYOUT_GENERAL,
						add_cref<Vec4>			background = Vec4(0),
						add_cref<Vec4>			foreground = Vec4(1),
						ZImageUsageFlags		imageUsage = ZImageUsageFlags(VK_IMAGE_USAGE_SAMPLED_BIT));
ZImage createTextImage (ZCommandPool			cmdPool,
						ZBuffer					fontBuffer,
						add_cref<std::string>	text,
						add_cref<VkExtent2D>	imageRect,
						add_cref<VkExtent2D>	charRect,
						const VkFormat format	= VK_FORMAT_R32G32B32A32_SFLOAT,
						const VkImageLayout		layout = VK_IMAGE_LAYOUT_GENERAL,
						add_cref<Vec4>			background = Vec4(0),
						add_cref<Vec4>			foreground = Vec4(1),
						ZImageUsageFlags		imageUsage = ZImageUsageFlags(VK_IMAGE_USAGE_SAMPLED_BIT),
						VkFormat				fontBufferFormat = VK_FORMAT_R8G8B8A8_UNORM);

} // namespace vtf

#endif // __VTF_TEXT_IMAGE_HPP_INCLUDED__
