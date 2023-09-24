#include "vtfBacktrace.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfTextImage.hpp"

#define FONT_FILE "font10x39_32-128.png"

namespace vtf
{

ZBuffer createTextImageFontBuffer (ZDevice device)
{
	const std::string fileName((fs::path(getGlobalAppFlags().assetsPath) / "__app__" / FONT_FILE).generic_u8string().c_str());
	return createBufferAndLoadFromImageFile(device, fileName, ZBufferUsageFlags());
}

ZImage createTextImage (ZCommandPool cmdPool, add_cref<std::string> text,
						add_cref<VkExtent2D> imageRect, add_cref<VkExtent2D> charRect,
						const VkFormat format, const VkImageLayout layout,
						add_cref<Vec4> background, add_cref<Vec4> foreground,
						ZImageUsageFlags imageUsage)
{
	ZBuffer fontBuffer = createTextImageFontBuffer(cmdPool.getParam<ZDevice>());
	return createTextImage(cmdPool, fontBuffer, text, imageRect, charRect,
						   format, layout, background, foreground, imageUsage, VK_FORMAT_R8G8B8A8_UNORM);
}

UNUSED static float mix(float a, float b, float f)
{
	return ((a * f) + ((1.0f - f) * b));
}

static uint8_t mix(uint8_t a, uint8_t b, float f)
{
	return uint8_t((float(a) * f) + ((1.0f - f) * float(b)));
}

static uint8_t mix(uint8_t a, uint8_t b, uint8_t f)
{
	return mix(a, b, (float(f) / 255.0f));
}

static void replaceColors (const VkFormat inFormat, ZBuffer inBuffer, ZBuffer outBuffer,
						   add_cref<Vec4> background, add_cref<Vec4> foreground,
						   bool alpha)
{
	UNREF(alpha);
	// TODO: format, etc, ...
	ASSERTMSG(inFormat == VK_FORMAT_R8G8B8A8_UNORM || inFormat == VK_FORMAT_R32G32B32A32_SFLOAT,
			  "Only VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R32G32B32A32_SFLOAT supported");
	const uint32_t pixelCount8 = static_cast<uint32_t>(bufferGetSize(inBuffer) / 4u);
	const UBVec4 bg8 = (background * 255.0f ).cast<UBVec4>();
	const UBVec4 fg8 = (foreground * 255.0f ).cast<UBVec4>();
	BufferTexelAccess<UBVec4> srcC8(inBuffer, pixelCount8, 1u, 1u);
	BufferTexelAccess<UBVec4> dstC8(outBuffer, pixelCount8, 1u, 1u);
	for (uint32_t x = 0u; x < pixelCount8; ++x)
	{
		add_cref<UBVec4> sColor = srcC8.at(x, 0u, 0u);
		add_ref<UBVec4> dColor = dstC8.at(x, 0u, 0u);
#if 0
		if (sColor.x() == 255)
			dColor = bg8;
		else
#endif
		{
			dColor.r(mix(bg8.r(), fg8.r(), sColor.r()));
			dColor.g(mix(bg8.g(), fg8.g(), sColor.g()));
			dColor.b(mix(bg8.b(), fg8.b(), sColor.b()));
			dColor.a(mix(bg8.a(), fg8.a(), sColor.a()));
		}
	}
}

ZImage createTextImage (ZCommandPool cmdPool, ZBuffer fontBuffer, add_cref<std::string> text,
						add_cref<VkExtent2D> imageRect, add_cref<VkExtent2D> charRect,
						const VkFormat format, const VkImageLayout layout,
						add_cref<Vec4> background, add_cref<Vec4> foreground,
						ZImageUsageFlags imageUsage, VkFormat fontBufferFormat)
{
	const int32_t		srcCharWidth	= 20;
	const int32_t		srcCharHeight	= 39;
	const VkOffset3D	textOrigin		= makeOffset3D(4,4);
	const int32_t		charHeight		= std::min(make_signed(imageRect.height) - textOrigin.y, make_signed(charRect.height));
	const uint32_t		charCount		= std::min((imageRect.width - make_unsigned(textOrigin.x)) / charRect.width, static_cast<uint32_t>(text.length()));
	for (uint32_t i = 0u; i < charCount; ++i)
	{
		ASSERTMSG(text[i] >= 32 && make_unsigned(text[i]) < 128u,
				  "Cannot create image with text, there are not supported characters");
	}

	ZBuffer				charBuffer	= bufferDuplicate(fontBuffer);
	replaceColors(fontBufferFormat, fontBuffer, charBuffer,background, foreground, true);

	ZDevice				device		= cmdPool.getParam<ZDevice>();
	const std::string	fileName	= (fs::path(getGlobalAppFlags().assetsPath) / "__app__" / FONT_FILE).generic_u8string().c_str();
	ZImage				charImage	= createImageFromFileMetadata(device, fileName);
	VkFilter			filter		= formatSupportsLinearFilter(deviceGetPhysicalDevice(device), format) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	ZImage				textImage	= createImage(device, format, VK_IMAGE_TYPE_2D, imageRect.width, imageRect.height, imageUsage);

	std::vector<VkImageBlit> blits(charCount);
	VkOffset3D dstOffset = textOrigin;
	for (uint32_t i = 0; i < charCount; ++i)
	{
		VkImageBlit& blit = blits[i];
		const uint32_t c = make_unsigned(text[i] - char(32));

		blit.srcSubresource		= imageMakeSubresourceLayers(charImage);
		blit.dstSubresource		= imageMakeSubresourceLayers(textImage);
		blit.srcOffsets[0].z	= blit.dstOffsets[0].z = 0;
		blit.srcOffsets[1].z	= blit.dstOffsets[1].z = 1;

		blit.srcOffsets[0].x	= (c % 16) * srcCharWidth;
		blit.srcOffsets[0].y	= (c / 16) * srcCharHeight;
		blit.srcOffsets[1].x	= blit.srcOffsets[0].x + srcCharWidth - 1;
		blit.srcOffsets[1].y	= blit.srcOffsets[0].y + srcCharHeight - 1;

		blit.dstOffsets[0].x	= dstOffset.x;
		blit.dstOffsets[0].y	= dstOffset.y;
		blit.dstOffsets[1].x	= dstOffset.x + make_signed(charRect.width);
		blit.dstOffsets[1].y	= dstOffset.y + charHeight;

		dstOffset.x += charRect.width;
	}

	ZImageMemoryBarrier preTextImage(textImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	ZImageMemoryBarrier postTextImage(textImage, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE, layout);

	OneShotCommandBuffer shot(cmdPool);
	ZCommandBuffer cmd = shot.commandBuffer;
	bufferCopyToImage(cmd, charBuffer, charImage,
					  VK_ACCESS_NONE, VK_ACCESS_NONE,
					  VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	commandBufferClearColorImage(cmd, textImage, makeClearColorValue(background));
	commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, preTextImage);
	vkCmdBlitImage(*cmd,
				   *charImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				   *textImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				   charCount, data_or_null(blits), filter);
	commandBufferPipelineBarriers(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, postTextImage);
	return textImage;
}

} // namespace vtf
