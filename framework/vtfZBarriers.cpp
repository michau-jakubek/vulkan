#include "vtfCUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBarriers.hpp"

namespace vtf
{

ZSubpassDependency::ZSubpassDependency	(VkAccessFlags srcAccess, VkAccessFlags dstAccess,
										 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
										 DependencyType type, VkDependencyFlags dependencyFlags, uint32_t multiViewMask)
	: m_multiViewMask(multiViewMask), m_type(type)
{
	add_ref<VkSubpassDependency> dep(*this);
	dep.srcSubpass		= 0u;
	dep.dstSubpass		= 0u;
	dep.srcStageMask	= srcStage;
	dep.dstStageMask	= dstStage;
	dep.srcAccessMask	= srcAccess;
	dep.dstAccessMask	= dstAccess;
	dep.dependencyFlags	= dependencyFlags;
}
VkSubpassDependency ZSubpassDependency::operator ()() const
{
	return static_cast<VkSubpassDependency>(*this);
}

ZMemoryBarrier::ZMemoryBarrier ()
{
	add_ref<VkMemoryBarrier> barrier(*this);
	barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
}

ZMemoryBarrier::ZMemoryBarrier (VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	add_ref<VkMemoryBarrier> barrier(*this);
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
}

VkMemoryBarrier ZMemoryBarrier::operator ()() const
{
	return static_cast<add_cref<VkMemoryBarrier>>(*this);
}

ZBufferMemoryBarrier::ZBufferMemoryBarrier ()
	: m_buffer ()
{
	add_ref<VkBufferMemoryBarrier>	barrier(*this);
	barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
}

ZBufferMemoryBarrier::ZBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess)
	: m_buffer (buffer)
{
	add_ref<VkBufferMemoryBarrier>	barrier(*this);

	barrier.sType				= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.srcAccessMask		= srcAccess;
	barrier.dstAccessMask		= dstAccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= *buffer;
	barrier.offset				= 0u;
	barrier.size				= buffer.getParam<VkDeviceSize>();
}

VkBufferMemoryBarrier ZBufferMemoryBarrier::operator ()() const
{
	return static_cast<add_cref<VkBufferMemoryBarrier>>(*this);
}

ZBuffer ZBufferMemoryBarrier::getBuffer () const
{
	return m_buffer;
}

ZBufferMemoryBarrier makeBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	return ZBufferMemoryBarrier(buffer, srcAccess, dstAccess);
}

ZImageMemoryBarrier::ZImageMemoryBarrier ()
	: m_image()
{
	add_ref<VkImageMemoryBarrier>	barrier(*this);
	barrier = { };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
}

ZImageMemoryBarrier::ZImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout)
	: ZImageMemoryBarrier(image, srcAccess, dstAccess, targetLayout, imageMakeSubresourceRange(image))
{
}

ZImageMemoryBarrier::ZImageMemoryBarrier (ZImage image,
										  VkAccessFlags srcAccess, VkAccessFlags dstAccess,
										  VkImageLayout targetLayout, add_cref<VkImageSubresourceRange> subresourceRange)
	: m_image(image)
{
	add_ref<VkImageMemoryBarrier>	barrier(*this);

	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.oldLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout			= targetLayout;
	barrier.srcAccessMask		= srcAccess;
	barrier.dstAccessMask		= dstAccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= *image;
	barrier.subresourceRange	= subresourceRange;
}

VkImageMemoryBarrier ZImageMemoryBarrier::operator()()
{
	VkImageMemoryBarrier barrier(static_cast<add_ref<VkImageMemoryBarrier>>(*this));
	barrier.oldLayout = imageGetLayout(m_image);
	imageResetLayout(m_image, barrier.newLayout);
	return barrier;
}

ZImage ZImageMemoryBarrier::getImage () const
{
	return m_image;
}

ZImageMemoryBarrier makeImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout)
{
	return ZImageMemoryBarrier(image, srcAccess, dstAccess, targetLayout);
}

void pushBarriers (add_ref<BarriersInfo>) { }
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZMemoryBarrier> barrier)
{
	info.pMemoryBarriers[info.memoryBarrierCount++] = barrier();
}

void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZBufferMemoryBarrier> barrier)
{
	info.pBufferBarriers[info.bufferBarrierCount++] = barrier();
	for (uint32_t i = 0; i < info.bufferBarrierCount; ++i)
	for (uint32_t j = i + 1; j < info.bufferBarrierCount; ++j)
	{
		ASSERTMSG(info.pBufferBarriers[i].buffer != info.pBufferBarriers[j].buffer,
				  "Buffer barriers must have different buffer handles, try different sizes or offsets");
	}
}
void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<ZImageMemoryBarrier> barrier)
{
	info.pImageBarriers[info.imageBarrierCount++] = barrier();
	for (uint32_t i = 0; i < info.imageBarrierCount; ++i)
	for (uint32_t j = i + 1; j < info.imageBarrierCount; ++j)
	{
		ASSERTMSG(info.pImageBarriers[i].image != info.pImageBarriers[j].image,
				  "Image barriers must have different image handles, try different layers or mip levels");
	}
}

void pushBarriers(
	add_ref<std::vector<VkMemoryBarrier>>,
	add_ref<std::vector<VkImageMemoryBarrier>>,
	add_ref<std::vector<VkBufferMemoryBarrier>>)
{
}

void pushKnownBarriers(
	add_ref<std::vector<VkMemoryBarrier>>		dstBarriers,
	add_ref<std::vector<VkImageMemoryBarrier>>,
	add_ref<std::vector<VkBufferMemoryBarrier>>,
	add_cref<std::vector<ZMemoryBarrier>>		srcBarriers)
{
	std::transform(srcBarriers.begin(), srcBarriers.end(), std::back_inserter(dstBarriers),
		[](add_cref<ZMemoryBarrier> barrier) { return barrier(); });
}

void pushKnownBarriers(
	add_ref<std::vector<VkMemoryBarrier>>,
	add_ref<std::vector<VkImageMemoryBarrier>>	dstBarriers,
	add_ref<std::vector<VkBufferMemoryBarrier>>,
	add_cref<std::vector<ZImageMemoryBarrier>>	srcBarriers)
{
	for (add_cref<ZImageMemoryBarrier> srcBarrier : srcBarriers)
	{
		if (srcBarrier.getImage().has_handle())
			dstBarriers.push_back(ZImageMemoryBarrier(srcBarrier)());
	}

	const uint32_t size = data_count(dstBarriers);
	for (uint32_t i = 0; i < size; ++i)
	for (uint32_t j = i + 1; j < size; ++j)
	{
		ASSERTMSG(dstBarriers[i].image != dstBarriers[j].image,
			"Image barriers must have different image handles, try different layers or mip levels");
	}
}

void pushKnownBarriers(
	add_ref<std::vector<VkMemoryBarrier>>,
	add_ref<std::vector<VkImageMemoryBarrier>>,
	add_ref<std::vector<VkBufferMemoryBarrier>>	dstBarriers,
	add_cref<std::vector<ZBufferMemoryBarrier>>	srcBarriers)
{
	for (add_cref<ZBufferMemoryBarrier> srcBarrier : srcBarriers)
	{
		if (srcBarrier.getBuffer().has_handle())
			dstBarriers.push_back(srcBarrier());
	}

	const uint32_t size = data_count(dstBarriers);
	for (uint32_t i = 0; i < size; ++i)
		for (uint32_t j = i + 1; j < size; ++j)
		{
			ASSERTMSG(dstBarriers[i].buffer != dstBarriers[j].buffer,
				"Buffer barriers must have different buffer handles, try different sizes or offsets");
		}

}

} // namespace vtf
