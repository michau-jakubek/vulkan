#ifndef __VTF_ZPIPELINE_HPP_INCLUDED__
#define __VTF_ZPIPELINE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfDSBMgr.hpp"
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
using DepthMinBounds		= ZDistType<DepthMinBounds, float>;
using DepthMaxBounds		= ZDistType<DepthMaxBounds, float>;
using StencilTestEnable		= ZDistType<StencilTestEnable, bool>;
using SubpassIndex			= ZDistType<SubpassIndex, uint32_t>;
using ViewportCount			= ZDistType<ViewportCount, uint32_t>;
using ScissorCount			= ZDistType<ScissorCount, uint32_t>;
using LineWidth				= ZDistType<LineWidth, float>;
using BlendAttachmentState	= ZDistType<BlendAttachmentState, std::pair<uint32_t, VkPipelineColorBlendAttachmentState>>;
using BlendConstants		= ZDistType<BlendConstants, Vec4>;
using SpecConstants			= ZDistType<SpecConstants, std::pair<VkShaderStageFlagBits, add_ref<ZSpecializationInfo>>>;
using PrimitiveRestart		= ZDistType<PrimitiveRestart, bool>;
using RasterizerDiscardEnable = ZDistType<RasterizerDiscardEnable, bool>;
using ViewMask				= ZDistType<ViewMask, uint32_t>;
enum AttachmentDesc { Presentation, Color, DeptStencil, Resolve, Input, Undefined };
struct AttachmentIndex
{
	uint32_t index;
	AttachmentIndex () : index(INVALID_UINT32) {}
	AttachmentIndex (uint32_t idx) : index(idx) {}
};
struct Attachment
{
	ZImageView		view;		// if has no handle then it will be treated as VK_ATTACHMENT_UNUSED
	AttachmentDesc	desc;
	uint32_t		index;		// if INVALID_UINT32 then it will be assigned in the order it appears in the list
	VkAttachmentLoadOp	loadOp;
	VkAttachmentStoreOp	storeOp;
	Attachment (ZImageView view_, AttachmentDesc desc_, AttachmentIndex index_ = {},
		VkAttachmentLoadOp loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR,
		VkAttachmentStoreOp storeOp_ = VK_ATTACHMENT_STORE_OP_STORE)
		: view(view_)
		, desc(desc_)
		, index(index_.index)
		, loadOp(loadOp_)
		, storeOp(storeOp_) {}
	Attachment () : Attachment(ZImageView(), AttachmentDesc::Color) {}
};
using RenderingAttachmentLocations		= ZDistType<RenderingAttachmentLocations, add_cptr<std::vector<uint32_t>>>;
using RenderingInpuAttachmentIndices	= ZDistType<RenderingInpuAttachmentIndices, add_cptr<std::vector<uint32_t>>>;

// VkExtent2D	sets both viewport and scissor
// VkViewport	sets viewport only
// VkRect2D		sets scissor only

} // namespace gpp

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
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::DepthWriteEnable>		enableDepthWrite);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::DepthMinBounds>		depthMinBounds);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::DepthMaxBounds>		depthMaxBounds);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkCompareOp							depthCompareOp);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::StencilTestEnable>	enableStencilTest);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::SubpassIndex>			subpassIndex);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, ZRenderPass							renderPass);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::BlendAttachmentState>	blendAttachmentState);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<std::vector<gpp::BlendAttachmentState>> blendAttachmentStates);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::BlendConstants>		blendConstants);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::SpecConstants>		specConstants);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::PrimitiveRestart>		primitiveRestartEnable);
void updateKnownSettings (add_ref<GraphicPipelineSettings>,	ZPipelineCache						pipelineCache);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::Attachment>			drAttachment);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<std::vector<gpp::Attachment>>	drAttachments);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::ViewMask>				drViewMask);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::RenderingAttachmentLocations>	locations);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::RenderingInpuAttachmentIndices>	indices);
void updateKnownSettings (add_ref<GraphicPipelineSettings>, add_cref<gpp::RasterizerDiscardEnable>	enable);

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

ZPipeline createComputePipeline(
	ZPipelineCache					pipelineCache,
	ZPipelineLayout					layout,
	ZShaderModule					computeShaderModule,
	add_ref<ZSpecializationInfo>	specInfo,
	bool							enableFullGroups = false);

inline ZPipeline createComputePipeline(
	ZPipelineLayout					layout,
	ZShaderModule					computeShaderModule,
	add_ref<ZSpecializationInfo>	specInfo,
	bool							enableFullGroups = false)
{
	return createComputePipeline(ZPipelineCache(), layout, computeShaderModule, specInfo, enableFullGroups);
}

// Please note that if any of localSize[?] is valid value and a layout of compute shader looks like
// layout(local_size_x_ID = X, local_size_y_ID = Y, local_size_z_ID = Z), then X,Y,Z will refer to
// the SpecID during compute pipeline creation. Be carefull to set them properly according to their
// index in localSize vector or left as negative value.
template<class... EntryTypes>
ZPipeline createComputePipeline (
	ZPipelineLayout				layout,
	ZShaderModule				computeShaderModule,
	ZPipelineCache				pipelineCache = {},
	add_cref<UVec3>				localSizes = UVec3(INVALID_UINT32),
	ZSpecEntry<EntryTypes>&&... entries)
{
	ZSpecializationInfo info;

	// Is 'const uvec3 gl_WorkGroupSize' really unsigned?
	// Validation layers and shader unfortunately see it as signed
	const uint32_t localSizes_x = localSizes.x();
	const uint32_t localSizes_y = localSizes.y();
	const uint32_t localSizes_z = localSizes.z();
	const bool autoLocalSizesIDs = (localSizes_x != INVALID_UINT32 && localSizes_x != 0u)
								|| (localSizes_y != INVALID_UINT32 && localSizes_y != 0u) 
								|| (localSizes_z != INVALID_UINT32 && localSizes_z != 0u);
	if (localSizes_x != INVALID_UINT32 && localSizes_x != 0u)
		info.addEntry(make_signed(localSizes_x), 0u);
	if (localSizes_y != INVALID_UINT32 && localSizes_y != 0u)
		info.addEntry(make_signed(localSizes_y), 1u);
	if (localSizes_z != INVALID_UINT32 || localSizes_y != 0u)
		info.addEntry(make_signed(localSizes_z), 2u);

	info.addEntries(entries...);

	extern ZPipeline createComputePipelineImpl(ZPipelineLayout layout, ZShaderModule computeShaderModule,
		ZPipelineCache pipelineCache, add_cref<UVec3>, bool autoLocalSizesIDs, bool enableFullGroups,
		add_ref<ZSpecializationInfo>);
	return createComputePipelineImpl(layout, computeShaderModule, pipelineCache,
										localSizes, autoLocalSizesIDs, false, info);
}

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline);

ZPipelineCache createPipelineCache(
	ZDevice						device,
	add_cref<std::string>		cacheFileName,
	VkPipelineCacheCreateFlags	flags = VkPipelineCacheCreateFlags(0),
	bool						forceReset = false);

} // namespace vtf

#endif // __VTF_ZPIPELINE_HPP_INCLUDED__
