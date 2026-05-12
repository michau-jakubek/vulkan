#ifndef __VTF_ZBARRIERS_HPP_INCLUDED__
#define __VTF_ZBARRIERS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{

struct ZMemoryBarrier : protected VkMemoryBarrier
{
	ZMemoryBarrier ();
	ZMemoryBarrier (VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	VkMemoryBarrier operator ()() const;
};

struct ZBufferMemoryBarrier : protected VkBufferMemoryBarrier
{
	ZBufferMemoryBarrier ();
	ZBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	VkBufferMemoryBarrier operator ()() const;
	ZBuffer getBuffer () const;
protected:
	ZBuffer	m_buffer;
};

struct ZImageMemoryBarrier : protected VkImageMemoryBarrier
{
	ZImageMemoryBarrier ();
	ZImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout);
	ZImageMemoryBarrier (ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout, add_cref<VkImageSubresourceRange>);
	// exchanges m_image layout according to barrier target layout
	VkImageMemoryBarrier operator()();
	ZImage getImage () const;
protected:
	ZImage	m_image;
};

ZBufferMemoryBarrier makeBufferMemoryBarrier (ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
ZImageMemoryBarrier makeImageMemoryBarrier	(ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout);

struct BarriersInfo
{
	add_ref<std::vector<VkMemoryBarrier>>		memoryBarriers;
	add_ref<std::vector<VkImageMemoryBarrier>>	imageBarriers;
	add_ref<std::vector<VkBufferMemoryBarrier>>	bufferBarriers;
	uint32_t						memoryBarrierCount;
	uint32_t						imageBarrierCount;
	uint32_t						bufferBarrierCount;
};

void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZMemoryBarrier> barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZBufferMemoryBarrier> barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<ZImageMemoryBarrier> barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<std::vector<ZMemoryBarrier>> barriers);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<std::vector<ZBufferMemoryBarrier>> barriers);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<std::vector<ZImageMemoryBarrier>> barriers);

void pushBarriers (add_ref<BarriersInfo>);
template<class Barrier, class... Barriers>
void pushBarriers (add_ref<BarriersInfo> info, Barrier&& barrier, Barriers&&... barriers)
{
	pushKnownBarrier(info, barrier);
	pushBarriers(info, std::forward<Barriers>(barriers)...);
}

} // namespace vtf

#endif // __VTF_ZBARRIERS_HPP_INCLUDED__
