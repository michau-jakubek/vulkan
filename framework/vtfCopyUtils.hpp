#ifndef __VTF_COPY_UTILS_HPP_INCLUDED__
#define __VTF_COPY_UTILS_HPP_INCLUDED__

#include "vtfZCommandBuffer.hpp"

namespace vtf
{

void				imageCopyToBuffer	(ZCommandPool commandPool, ZImage image, ZBuffer buffer);
void				imageCopyToBuffer	(ZCommandBuffer commandBuffer, ZImage image, ZBuffer buffer,
										 VkAccessFlags srcImageAccess, VkAccessFlags dstImageAccess,
										 VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
										 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
										 VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
										 uint32_t baseLevel = 0u, uint32_t levelCount = 1u,
										 uint32_t baseLayer = 0u, uint32_t layerCount = 1u);
void				imageCopyToBuffer2 (ZCommandBuffer commandBuffer,
										ZImage image, ZBuffer buffer,
										ZBarrierConstants::Access srcImageAccess,  ZBarrierConstants::Access dstImageAccess,
										ZBarrierConstants::Access srcBufferAccess, ZBarrierConstants::Access dstBufferAccess,
										ZBarrierConstants::Stage srcStage, ZBarrierConstants::Stage dstStage,
										VkImageLayout targetImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
										uint32_t baseLevel = 0u, uint32_t levelCount = 1u,
										uint32_t baseLayer = 0u, uint32_t layerCount = 1u);
void				bufferCopyToBuffer	(ZCommandPool commandPool, ZBuffer srcBuffer, ZBuffer dstBuffer);
void				bufferCopyToBuffer	(ZCommandBuffer cmdBuffer, ZBuffer srcBuffer, ZBuffer dstBuffer,
										 VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
										 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);
void				bufferCopyToBuffer2 (ZCommandBuffer cmdBuffer, ZBuffer srcBuffer, ZBuffer dstBuffer,
										 ZBarrierConstants::Access srcBufferAccess, ZBarrierConstants::Access dstBufferAccess,
										 ZBarrierConstants::Stage srcStage, ZBarrierConstants::Stage dstStage);
void				bufferCopyToImage	(ZCommandBuffer cmdBuffer, ZBuffer buffer, ZImage image,
										 VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
										 VkAccessFlags srcImageAccess, VkAccessFlags dstImageAccess,
										 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
										 VkImageLayout finalImageLayout = VK_IMAGE_LAYOUT_GENERAL,
										 uint32_t baseLevel = 0u, uint32_t levelCount = 1u,
										 uint32_t baseLayer = 0u, uint32_t layerCount = 1u);
void				imageCopyToImage	(ZCommandBuffer cmdBuffer, ZImage srcImage, ZImage dstImage,
										 uint32_t srcArrayLayer, uint32_t arrayLayers, uint32_t dstArrayLayer,
										 uint32_t srcMipLevel, uint32_t mipLevels, uint32_t dstMipLevel,
										 VkAccessFlags inSrcAccess, VkAccessFlags outSrcAccess,
										 VkAccessFlags inDstAccess, VkAccessFlags outDstAccess,
										 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
										 VkImageLayout finalSrcLayout, VkImageLayout finalDstLayout);
void				imageCopyToImage2	(ZCommandBuffer cmdBuffer, ZImage srcImage, ZImage dstImage,
										 ZBarrierConstants::Access inSrcAccess, ZBarrierConstants::Access outSrcAccess,
										 ZBarrierConstants::Access inDstAccess, ZBarrierConstants::Access outDstAccess,
										 ZBarrierConstants::Stage srcStage, ZBarrierConstants::Stage dstStage,
										 VkImageLayout finalSrcLayout, VkImageLayout finalDstLayout,
										 uint32_t srcArrayLayer = 0, uint32_t arrayLayers = 1, uint32_t dstArrayLayer = 0,
										 uint32_t srcMipLevel = 0, uint32_t mipLevels = 1, uint32_t dstMipLevel = 0);


} // namespace vtf

#endif // __VTF_COPY_UTILS_HPP_INCLUDED__
