#ifndef __VTF_ZRENDER_PASS_2_HPP_INCLUDED__
#define __VTF_ZRENDER_PASS_2_HPP_INCLUDED__

#include "vtfZUtils.hpp"
#include "vtfZPipeline.hpp"
#include "vtfZBarriers2.hpp"

namespace vtf
{

using gpp::AttachmentDesc;
class ZAttachmentPool;
struct ZSubpassDescription2;

/// <summary>
/// RPAR - Render Pass Attachment Reference
/// first: the attachment index in the vector passed to the pool
/// second: equivalent to VkAttachmentReference2::attachment field
/// </summary>
struct RPAR : public std::pair<uint32_t, VkImageLayout>
{
	AttachmentDesc cast;
	explicit RPAR(uint32_t index, AttachmentDesc cast_ = AttachmentDesc::Undefined)
		: std::pair<uint32_t, VkImageLayout>(index, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL), cast(cast_) {}
	explicit RPAR(uint32_t index, VkImageLayout subpassLayout, AttachmentDesc cast_ = AttachmentDesc::Undefined)
		: std::pair<uint32_t, VkImageLayout>(index, subpassLayout), cast(cast_) {}
};
using RPARS = std::vector<RPAR>;

struct RPA : VkAttachmentDescription2
{
	uint32_t mIndex;
	VkClearValue mClearValue;
	AttachmentDesc mDescription;
	VkImageUsageFlags mAdditionalUsage;

	// if finalLayout == MAX_ENUM then it is set to VK_IMAGE_LAYOUT_(COLOR|DEPTH)_ATTACHMENT_OPTIMAL
// depending on the format.

	RPA(AttachmentDesc desc, uint32_t otherRpaIndex);
	RPA(AttachmentDesc desc, VkFormat format, VkClearValue clearValue,
		VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
		VkImageUsageFlags additionalUsage = 0);
	RPA(AttachmentDesc desc, VkFormat format,
		VkSampleCountFlagBits samples, VkClearValue clearValue,
		VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM,
		VkImageUsageFlags additionalUsage = 0);

	VkAttachmentDescription2 operator()() const;
	static bool isResourceAttachment (AttachmentDesc desc);
	static add_cptr<char> descToString (AttachmentDesc desc);
};

typedef std::pair<uint32_t, RPA> Item;
typedef std::vector<Item> ItemVec;

void freeAttachmentPool (add_ptr<ZAttachmentPool>);
class ZAttachmentPool : public
	ZDeletable<add_ptr<ZAttachmentPool>, decltype(&freeAttachmentPool), &freeAttachmentPool,
	swizzle_one_param, ZDeletableBase,
	std::vector<std::pair<uint32_t, RPA>>,
	uint32_t>
{
	static inline uint32_t seed;
public:
	ZAttachmentPool (add_cref<std::vector<RPA>> list);
	void updateDescriptions (add_ref<std::vector<VkAttachmentDescription2>> descs) const;
	void updateReferences (add_cref<RPARS> referenceIndices,
						   add_ref<std::vector<VkAttachmentReference>> inputAttachments,
						   add_ref<uint32_t> inputCount, uint32_t inputOffset,
						   add_ref<std::vector<VkAttachmentReference>> colorAttachments,
						   add_ref<uint32_t> colorCount, uint32_t colorOffset,
						   add_ref<std::vector<VkAttachmentReference>> resolveAttachments,
						   add_ref<uint32_t> resolveCount, uint32_t resolveOffset,
						   add_ref<std::vector<VkAttachmentReference>> dsAttachments,
						   add_ref<uint32_t> dsCount, uint32_t dsOffset) const;
	void updateReferences (add_cref<RPARS> referenceIndices,
						   add_ref<std::vector<VkAttachmentReference2>> inputAttachments,
						   add_ref<uint32_t> inputCount, uint32_t inputOffset,
						   add_ref<std::vector<VkAttachmentReference2>> colorAttachments,
						   add_ref<uint32_t> colorCount, uint32_t colorOffset,
						   add_ref<std::vector<VkAttachmentReference2>> resolveAttachments,
						   add_ref<uint32_t> resolveCount, uint32_t resolveOffset,
						   add_ref<std::vector<VkAttachmentReference2>> dsAttachments,
						   add_ref<uint32_t> dsCount, uint32_t dsOffset) const;
	auto updateClearValues (add_ref<std::vector<VkClearValue>> clearValues) const -> uint32_t;
	VkAttachmentDescription2 get (uint32_t at, add_ref<uint32_t> realIndex) const;
	add_cref<RPA> raw (uint32_t at, add_ref<uint32_t> realIndex) const;
	uint32_t getPresentationIndex () const;
	uint32_t count (add_cref<RPARS> subpassAttachments, std::initializer_list<AttachmentDesc> descs) const;
	uint32_t count (std::initializer_list<AttachmentDesc> descs) const;
	uint32_t size () const;
	uint32_t id () const;
};

void freeSubpassDescription2 (add_ptr<ZSubpassDescription2>);
struct ZSubpassDescription2 : public
	ZDeletable<add_ptr<ZSubpassDescription2>, decltype(&freeSubpassDescription2), &freeSubpassDescription2,
	swizzle_one_param, ZDeletableBase,
	Flags<VkSubpassDescriptionFlags, VkSubpassDescriptionFlagBits>,
	VkPipelineBindPoint
	, uint32_t /*viewMask*/
	, RPARS /*attachmentReferences*/
	, ZDistType<SomeZero, uint32_t> /*prevSubpass*/
	, ZDistType<SomeOne, uint32_t> /*nextSubpass*/
>
{
public:
	using ZSubpassDescriptionFlags =
		Flags<VkSubpassDescriptionFlags, VkSubpassDescriptionFlagBits>;
	ZSubpassDescription2 (add_cref<RPARS> attachmentReferences,
						  VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
						  uint32_t viewMask = 0u,
						  VkSubpassDescriptionFlags flags = 0);
};

struct ZSubpassDependency2: public VkSubpassDependency2
{
	ZSubpassDependency2(VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_NONE,
						VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_NONE,
						VkAccessFlags        srcAccessMask = VK_ACCESS_NONE,
						VkAccessFlags        dstAccessMask = VK_ACCESS_NONE,
						VkDependencyFlags    dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
						int32_t              viewOffset = 0);
	VkSubpassDependency2 operator()() const;
};

namespace details
{
void pushRenderpassParam (
	add_ref<std::vector<ZSubpassDescription2>> descs,
	add_ref<std::vector<ZSubpassDependency2>> deps,
	ZSubpassDependency2 const&);
void pushRenderpassParam (
	add_ref<std::vector<ZSubpassDescription2>> descs,
	add_ref<std::vector<ZSubpassDependency2>> deps,
	ZSubpassDescription2 const&);
void pushRenderpassParams(
	add_ref<std::vector<ZSubpassDescription2>> descs,
	add_ref<std::vector<ZSubpassDependency2>> deps);

template<class SubpassDescOrDep, class... SubpassDescsOrDeps>
void pushRenderpassParams (
	add_ref<std::vector<ZSubpassDescription2>> descs,
	add_ref<std::vector<ZSubpassDependency2>> deps,
	SubpassDescOrDep const& dd, SubpassDescsOrDeps const&... others)
{
	pushRenderpassParam(descs, deps, dd);
	pushRenderpassParams(descs, deps, others...);
}
};

uint32_t renderPassGetAttachmentCount (ZRenderPass renderPass, uint32_t subpass);
uint32_t renderPassSubpassGetColorAttachmentCount (ZRenderPass renderPass, uint32_t subpass);

template<class T>
using IsDescOrDep = std::bool_constant<std::is_same_v<T, ZSubpassDescription2>
									|| std::is_same_v<T, ZSubpassDependency2>>;

template<class SubpassDescOrDep, class... SubpassDescsOrDeps>
ZRenderPass createRenderPass2 (
	ZDevice device, ZAttachmentPool pool,
	SubpassDescOrDep const& dd, SubpassDescsOrDeps const&... others)
{
	static_assert(
		std::conjunction_v<IsDescOrDep<SubpassDescOrDep>, IsDescOrDep<SubpassDescsOrDeps>...>,
		"All types must be either ZSubpassDescription2 or ZSubpassDependency2"
		);

	extern ZRenderPass createRenderPassImpl2 (
		ZDevice, ZAttachmentPool,
		uint32_t, add_ptr<ZSubpassDescription2>,
		uint32_t, add_ptr<ZSubpassDependency2>);
	std::vector<ZSubpassDescription2> descriptions;
	std::vector<ZSubpassDependency2> dependencies;
	details::pushRenderpassParams(descriptions, dependencies, dd, others...);
	return createRenderPassImpl2(device, pool,
								data_count(descriptions), data_or_null(descriptions),
								data_count(dependencies), data_or_null(dependencies));
}

} // namespace vtf

#endif // __VTF_ZRENDER_PASS_HPP_2_INCLUDED__
