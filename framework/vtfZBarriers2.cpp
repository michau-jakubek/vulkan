#include "vtfBacktrace.hpp"
#include "vtfZBarriers2.hpp"
#include "vtfZImage.hpp"

namespace vtf
{

ZMemoryBarrier2::ZMemoryBarrier2 (add_ptr<void> pNext)
{
	add_ref<VkMemoryBarrier2> barrier(*this);
	barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.pNext = pNext;
}

ZMemoryBarrier2::ZMemoryBarrier2 (Access srcAccess, Stage srcStage, Access dstAccess, Stage dstStage, add_ptr<void> pNext)
{
	add_ref<VkMemoryBarrier2> barrier(*this);
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.pNext = pNext;
	barrier.srcAccessMask = static_cast<VkAccessFlags2>(srcAccess);
	barrier.dstAccessMask = static_cast<VkAccessFlags2>(dstAccess);
	barrier.srcStageMask = static_cast<VkPipelineStageFlags2>(srcStage);
	barrier.dstStageMask = static_cast<VkPipelineStageFlags2>(dstStage);
}

VkMemoryBarrier2 ZMemoryBarrier2::operator ()()
{
	return static_cast<add_ref<VkMemoryBarrier2>>(*this);
}


ZBufferMemoryBarrier2::ZBufferMemoryBarrier2 (add_ptr<void> pNext)
	: m_buffer ()
{
	add_ref<VkBufferMemoryBarrier2>	barrier(*this);
	barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.pNext = pNext;
}

ZBufferMemoryBarrier2::ZBufferMemoryBarrier2 (ZBuffer buffer,	Access srcAccess, Stage srcStage,
																Access dstAccess, Stage dstStage, add_ptr<void> pNext)
	: m_buffer (buffer)
{
	add_ref<VkBufferMemoryBarrier2>	barrier(*this);

	barrier.sType				= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.pNext				= pNext;
	barrier.srcAccessMask		= static_cast<VkAccessFlags>(srcAccess);
	barrier.srcStageMask		= static_cast<VkPipelineStageFlags2>(srcStage);
	barrier.dstAccessMask		= static_cast<VkAccessFlags2>(dstAccess);
	barrier.dstStageMask		= static_cast<VkPipelineStageFlags2>(dstStage);
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= *buffer;
	barrier.offset				= 0u;
	barrier.size				= buffer.getParam<VkDeviceSize>();
}

VkBufferMemoryBarrier2 ZBufferMemoryBarrier2::operator ()()
{
	return static_cast<add_ref<VkBufferMemoryBarrier2>>(*this);
}


ZImageMemoryBarrier2::ZImageMemoryBarrier2 (add_ptr<void> pNext)
	: m_image()
{
	add_ref<VkImageMemoryBarrier2>	barrier(*this);
	barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.pNext = pNext;
}

ZImageMemoryBarrier2::ZImageMemoryBarrier2 (ZImage image,	Access srcAccess, Stage srcStage,
															Access dstAccess, Stage dstStage, VkImageLayout targetLayout,
															add_ptr<void> pNext)
	: ZImageMemoryBarrier2(image, srcAccess, srcStage, dstAccess, dstStage, targetLayout, imageMakeSubresourceRange(image), pNext)
{
}

ZImageMemoryBarrier2::ZImageMemoryBarrier2 (ZImage image,
											Access srcAccess, Stage srcStage,
											Access dstAccess, Stage dstStage,
											VkImageLayout targetLayout, add_cref<VkImageSubresourceRange> subresourceRange,
											add_ptr<void> pNext)
	: m_image(image)
{
	add_ref<VkImageMemoryBarrier2>	barrier(*this);

	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.pNext				= pNext;
	barrier.oldLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout			= targetLayout;
	barrier.srcAccessMask		= static_cast<VkAccessFlags2>(srcAccess);
	barrier.srcStageMask		= static_cast<VkPipelineStageFlags2>(srcStage);
	barrier.dstAccessMask		= static_cast<VkAccessFlags2>(dstAccess);
	barrier.dstStageMask		= static_cast<VkPipelineStageFlags2>(dstStage);
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= *image;
	barrier.subresourceRange	= subresourceRange;
}

VkImageMemoryBarrier2 ZImageMemoryBarrier2::operator ()()
{
	VkImageMemoryBarrier2 barrier(static_cast<add_ref<VkImageMemoryBarrier2>>(*this));
	barrier.oldLayout = imageGetLayout(m_image);
	imageResetLayout(m_image, barrier.newLayout);
	return barrier;
}


void pushBarriers (add_ref<BarriersInfo2>) { }
void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZMemoryBarrier2>& barrier)
{
	info.pMemoryBarriers[info.memoryBarrierCount++] = barrier();
}

void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZBufferMemoryBarrier2> barrier)
{
	info.pBufferBarriers[info.bufferBarrierCount++] = barrier();
	for (uint32_t i = 0; i < info.bufferBarrierCount; ++i)
	for (uint32_t j = i + 1; j < info.bufferBarrierCount; ++j)
	{
		ASSERTMSG(info.pBufferBarriers[i].buffer != info.pBufferBarriers[j].buffer,
				  "Buffer barriers must have different buffer handles, try different sizes or offsets");
	}
}
void pushKnownBarrier (add_ref<BarriersInfo2> info, add_ref<ZImageMemoryBarrier2> barrier)
{
	info.pImageBarriers[info.imageBarrierCount++] = barrier();
	for (uint32_t i = 0; i < info.imageBarrierCount; ++i)
	for (uint32_t j = i + 1; j < info.imageBarrierCount; ++j)
	{
		ASSERTMSG(info.pImageBarriers[i].image != info.pImageBarriers[j].image,
				  "Image barriers must have different image handles, try different layers or mip levels");
	}
}

void doCommandBufferPipelineBarriers2 (ZCommandBuffer			cmd,
									   add_cref<BarriersInfo2>	info,
									   VkDependencyFlags		dependencyFlags)
{
	// TODO: verify VK_KHR_synchronization2 and api >= 1.3
	add_ref<strings> exts = cmd.getParam<ZDevice>().getParam<ZPhysicalDevice>().getParamRef<strings>();
	ASSERTMSG(containsString(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, exts),
			  VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME " not supported by device");
	ASSERTMSG(!(getGlobalAppFlags().apiVer < Version(1,3)),
			  "Improper Vulkan API version, expected >= 1.3. Try with -api 13");

	VkDependencyInfo dependencyInfo
	{
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO,							// sType
		nullptr,													// pNext
		dependencyFlags,											// dependencyFlags;
		info.memoryBarrierCount,									// memoryBarrierCount
		info.memoryBarrierCount ? info.pMemoryBarriers : nullptr,	// pMemoryBarriers
		info.bufferBarrierCount,									// bufferMemoryBarrierCount
		info.bufferBarrierCount ? info.pBufferBarriers : nullptr,	// pBufferMemoryBarriers
		info.imageBarrierCount,										// imageMemoryBarrierCount
		info.imageBarrierCount ? info.pImageBarriers : nullptr		// pImageMemoryBarriers
	};

	vkCmdPipelineBarrier2(*cmd, &dependencyInfo);
}

} // namespace vtf
