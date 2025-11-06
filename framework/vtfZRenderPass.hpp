#ifndef __VTF_ZRENDER_PASS_HPP_INCLUDED__
#define __VTF_ZRENDER_PASS_HPP_INCLUDED__

#include "vtfZRenderPass2.hpp"

namespace vtf
{

ZRenderPass createSinglePresentationRenderPass(ZDevice device, VkFormat presentationImageFormat, add_cref<VkClearValue> = {});

template<class SubpassDescOrDep, class... SubpassDescsOrDeps>
ZRenderPass createRenderPass (
	ZDevice device, ZAttachmentPool pool,
	SubpassDescOrDep const& dd, SubpassDescsOrDeps const&... others)
{
	static_assert(
		std::conjunction_v<IsDescOrDep<SubpassDescOrDep>, IsDescOrDep<SubpassDescsOrDeps>...>,
		"All types must be either ZSubpassDescription2 or ZSubpassDependency2"
		);

	extern ZRenderPass createRenderPassImpl (
		ZDevice, ZAttachmentPool,
		uint32_t, add_ptr<ZSubpassDescription2>,
		uint32_t, add_ptr<ZSubpassDependency2>);
	std::vector<ZSubpassDescription2> descriptions;
	std::vector<ZSubpassDependency2> dependencies;
	details::pushRenderpassParams(descriptions, dependencies, dd, others...);
	return createRenderPassImpl(device, pool,
								data_count(descriptions), data_or_null(descriptions),
								data_count(dependencies), data_or_null(dependencies));
}

ZFramebuffer _createFramebuffer (VkImage presentImage, ZRenderPass renderpass,
									uint32_t width, uint32_t height, VkImageUsageFlags swapchainImageUsage);

} // namespace vtf

#endif // __VTF_ZRENDER_PASS_HPP_INCLUDED__
