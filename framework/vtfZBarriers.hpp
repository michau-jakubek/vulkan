#ifndef __VTF_ZBARRIERS_HPP_INCLUDED__
#define __VTF_ZBARRIERS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{

struct ZSubpassDependency : VkSubpassDependency
{
	ZSubpassDependency (VkAccessFlags			srcAccess,	VkAccessFlags			dstAccess,
						VkPipelineStageFlags	srcStage,	VkPipelineStageFlags	dstStage,
						VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT);
	VkSubpassDependency operator ()();
};

struct ZMemoryBarrier : protected VkMemoryBarrier
{
	ZMemoryBarrier ();
	ZMemoryBarrier (VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	VkMemoryBarrier operator ()();
};

struct ZBufferMemoryBarrier : protected VkBufferMemoryBarrier
{
	ZBufferMemoryBarrier ();
	ZBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	VkBufferMemoryBarrier operator()();
protected:
	ZBuffer	m_buffer;
};

struct ZImageMemoryBarrier : protected VkImageMemoryBarrier
{
	ZImageMemoryBarrier ();
	ZImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout);
	ZImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout, add_cref<VkImageSubresourceRange>);
	VkImageMemoryBarrier operator()();
protected:
	ZImage	m_image;
};

ZBufferMemoryBarrier makeBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
ZImageMemoryBarrier makeImageMemoryBarrier	(ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout);

struct BarriersInfo
{
	add_ptr<VkMemoryBarrier>		pMemoryBarriers;
	add_ptr<VkImageMemoryBarrier>	pImageBarriers;
	add_ptr<VkBufferMemoryBarrier>	pBufferBarriers;
	uint32_t						memoryBarrierCount;
	uint32_t						imageBarrierCount;
	uint32_t						bufferBarrierCount;
};

void pushKnownBarrier (add_ref<BarriersInfo> info, ZMemoryBarrier& barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, ZBufferMemoryBarrier& barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, ZImageMemoryBarrier& barrier);

void pushBarriers (add_ref<BarriersInfo>);
template<class Barrier, class... Barriers>
void pushBarriers (add_ref<BarriersInfo> info, Barrier&& barrier, Barriers&&... barriers)
{
	pushKnownBarrier(info, barrier);
	pushBarriers(info, std::forward<Barriers>(barriers)...);
}

} // namespace vtf

#endif // __VTF_ZBARRIERS_HPP_INCLUDED__
