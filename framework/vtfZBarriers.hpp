#ifndef __VTF_ZBARRIERS_HPP_INCLUDED__
#define __VTF_ZBARRIERS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{

struct ZSubpassDependency : protected VkSubpassDependency
{
	enum DependencyType
	{
		Begin,
		Between,
		Self,
		End
	};

	ZSubpassDependency (VkAccessFlags			srcAccess,	VkAccessFlags			dstAccess,
						VkPipelineStageFlags	srcStage,	VkPipelineStageFlags	dstStage,
						DependencyType		dependencyType,
						VkDependencyFlags	dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
						uint32_t			multiViewMask	= 0u);
	static ZSubpassDependency makeBegin (uint32_t multiViewMask = 0u) {
		return ZSubpassDependency(VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT,
								  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								  ZSubpassDependency::Begin,
								  VK_DEPENDENCY_BY_REGION_BIT, multiViewMask);
	}
	static ZSubpassDependency makeEnd (uint32_t multiViewMask = 0u) {
		return ZSubpassDependency(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_NONE,
								  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
								  ZSubpassDependency::End,
								  VK_DEPENDENCY_BY_REGION_BIT, multiViewMask);
	}
	VkSubpassDependency operator ()() const;
	uint32_t			getMultiViewMask () const { return m_multiViewMask; }
	DependencyType		getType () const { return m_type; }
protected:
	uint32_t		m_multiViewMask;
	DependencyType	m_type;
};

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

void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZMemoryBarrier> barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_cref<ZBufferMemoryBarrier> barrier);
void pushKnownBarrier (add_ref<BarriersInfo> info, add_ref<ZImageMemoryBarrier> barrier);

void pushBarriers (add_ref<BarriersInfo>);
template<class Barrier, class... Barriers>
void pushBarriers (add_ref<BarriersInfo> info, Barrier&& barrier, Barriers&&... barriers)
{
	pushKnownBarrier(info, barrier);
	pushBarriers(info, std::forward<Barriers>(barriers)...);
}

} // namespace vtf

#endif // __VTF_ZBARRIERS_HPP_INCLUDED__
