#include "vtfCUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBarriers.hpp"

namespace vtf
{

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
	if (info.memoryBarrierCount < info.memoryBarriers.size())
		info.memoryBarriers[info.memoryBarrierCount++] = barrier();
	else {
		info.memoryBarriers.push_back(barrier());
		++info.memoryBarrierCount;
	}
}
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<std::vector<ZMemoryBarrier>> barriers)
{
	for (add_cref<ZMemoryBarrier> b : barriers) pushKnownBarrier(info, b);
}

void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZBufferMemoryBarrier> barrier)
{
	if (info.bufferBarrierCount < info.bufferBarriers.size())
		info.bufferBarriers[info.bufferBarrierCount++] = barrier();
	else {
		info.bufferBarriers.push_back(barrier());
		++info.bufferBarrierCount;
	}
}
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<std::vector<ZBufferMemoryBarrier>> barriers)
{
	for (add_cref<ZBufferMemoryBarrier> b : barriers) pushKnownBarrier(info, b);
}

void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<ZImageMemoryBarrier> barrier)
{
	if (info.imageBarrierCount < info.imageBarriers.size())
		info.imageBarriers[info.imageBarrierCount++] = barrier();
	else {
		info.imageBarriers.push_back(barrier());
		++info.imageBarrierCount;
	}
}
void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<std::vector<ZImageMemoryBarrier>> barriers)
{
	for (add_ref<ZImageMemoryBarrier> b : barriers) pushKnownBarrier(info, b);
}

} // namespace vtf
