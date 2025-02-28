#ifndef __VTF_ZPIPELINE_HPP_INCLUDED__
#define __VTF_ZPIPELINE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfLayoutManager.hpp"
#include "vtfVertexInput.hpp"
#include "vtfZSpecializationInfo.hpp"
#include <memory>
#include <tuple>

namespace vtf
{

struct GraphicPipelineSettings;

namespace gpp // Graphics Pipeline Param
{

constexpr VkColorComponentFlags defaultBlendAttachmentColorWriteMask =
	(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

constexpr VkPipelineColorBlendAttachmentState defaultBlendAttachmentState = {
	VK_FALSE,					// VkBool32                 blendEnable;
	VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor;
	VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor;
	VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp;
	VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor;
	VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor;
	VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp;
	defaultBlendAttachmentColorWriteMask	//				colorWriteMask;
};

using PatchControlPoints	= ZDistType<PatchControlPoints, uint32_t>;
using CullModeFlags			= ZDistType<CullModeFlags, VkCullModeFlags>;
using DepthTestEnable		= ZDistType<DepthTestEnable, bool>;
using DepthWriteEnable		= ZDistType<DepthWriteEnable, bool>;
using StencilTestEnable		= ZDistType<StencilTestEnable, bool>;
using SubpassIndex			= ZDistType<SubpassIndex, uint32_t>;
using ViewportCount			= ZDistType<ViewportCount, uint32_t>;
using ScissorCount			= ZDistType<ScissorCount, uint32_t>;
using LineWidth				= ZDistType<LineWidth, float>;
using AttachmentCount		= ZDistType<AttachmentCount, uint32_t>;
using BlendAttachmentState	= ZDistType<BlendAttachmentState, std::pair<uint32_t, VkPipelineColorBlendAttachmentState>>;
using BlendConstants		= ZDistType<BlendConstants, Vec4>;
using SpecConstants			= ZDistType<SpecConstants, std::pair<VkShaderStageFlagBits, add_ref<ZSpecializationInfo>>>;

	// VkExtent2D	sets both viewport and scissor
	// VkViewport	sets viewport only
	// VkRect2D		sets scissor only
} // gpp

void updateKnownSettings (add_ref<GraphicPipelineSettings>, ZPipelineCreateFlags				createFlags);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, ZShaderModule						shaderModule);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<VertexBinding>				vertexBinding);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<VertexInput>				vertexInput);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkPrimitiveTopology					topology);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::PatchControlPoints>	patchControlPoints);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkDynamicState						dynamicState);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::ViewportCount>		viewportCount);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::ScissorCount>			scissorCount);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkPolygonMode						polygonMode);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::LineWidth>			lineWidth);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::CullModeFlags>		cullModeFlags);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkFrontFace							frontFace);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkSampleCountFlagBits				sampleCount);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<VkExtent2D>				viewportAndScissor);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<VkViewport>				viewport);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<VkRect2D>					scissor);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::DepthTestEnable>		enableDepthTest);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::DepthWriteEnable>		enableDepthMask);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::StencilTestEnable>	enableStencilTest);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::SubpassIndex>			subpassIndex);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, ZRenderPass							renderPass);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::AttachmentCount>		attachmentCount);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::BlendAttachmentState>	blendAttachmentState);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::BlendConstants>		blendConstants);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::SpecConstants>		specConstants);

// end of template recursion
void updateSettings (add_ref<GraphicPipelineSettings>);

template<class Y, class... X>
void updateSettings (add_ref<GraphicPipelineSettings> settings, Y&& param, X&&... params)
{
	updateKnownSettings(settings, param);
	updateSettings(settings, std::forward<X>(params)...);
}

std::shared_ptr<GraphicPipelineSettings> makeGraphicsPipelineSettings (ZPipelineLayout layout);

ZPipeline createGraphicsPipeline (add_ref<GraphicPipelineSettings> settings);

template<class... X>
ZPipeline createGraphicsPipeline (ZPipelineLayout layout, X&&... params)
{
	std::shared_ptr<GraphicPipelineSettings> settings = makeGraphicsPipelineSettings(layout);
	updateSettings(*settings, std::forward<X>(params)...);
	return createGraphicsPipeline(*settings);
}

bool computePipelineVerifyLimits (ZDevice device, add_cref<UVec3> wgSizes, bool raise = true);
ZPipeline createComputePipeline (
	ZPipelineLayout					layout,
	ZShaderModule					computeShaderModule,
	add_ref<ZSpecializationInfo>	specInfo,
	bool							enableFullGroups		= false);
// Please note that if any of localSize[?] is valid value and a layout of compute shader looks like
// layout(local_size_x_ID = X, local_size_y_ID = Y, local_size_z_ID = Z), then X,Y,Z will refer to
// the SpecID during compute pipeline creation. Be carefull to set them properly according to their
// index in localSize vector or left as negative value.
template<class... EntryTypes>
ZPipeline createComputePipeline (
	ZPipelineLayout				layout,
	ZShaderModule				computeShaderModule,
	add_cref<UVec3>				localSizes = UVec3(INVALID_UINT32),
	ZSpecEntry<EntryTypes>&&... entries)
{
	ZSpecializationInfo info;

	// Is 'const uvec3 gl_WorkGroupSize' really unsigned?
	// Validation layers and shader unfortunately see it as signed.
	const bool autoLocalSizesIDs = make_signed(localSizes.x()) >= 0
								|| make_signed(localSizes.y()) >= 0
								|| make_signed(localSizes.z()) >= 0;
	if (autoLocalSizesIDs)
	{
		info.addEntries(ZSpecEntry<int32_t>(make_signed(localSizes.x()), 0u),
						ZSpecEntry<int32_t>(make_signed(localSizes.y()), 1u),
						ZSpecEntry<int32_t>(make_signed(localSizes.z()), 2u));
	}

	info.addEntries(entries...);

	extern ZPipeline createComputePipelineImpl(ZPipelineLayout layout, ZShaderModule computeShaderModule,
		add_cref<UVec3>, bool autoLocalSizesIDs, bool enableFullGroups, add_ref<ZSpecializationInfo>);
	return createComputePipelineImpl(layout, computeShaderModule, localSizes, autoLocalSizesIDs, false, info);
}

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline);

} // namespace vtf

#endif // __VTF_ZPIPELINE_HPP_INCLUDED__
