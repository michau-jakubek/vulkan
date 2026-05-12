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

struct Nope {/*struct that does nothong*/};
} // namespace gpp

void updateKnownSettings (add_ref<GraphicPipelineSettings>, gpp::Nope);
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
void updateKnownSettings (add_ref<GraphicPipelineSettings>, VkCullModeFlagBits					cullMode);
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

template<class... X>
ZPipeline createGraphicsPipeline (ZPipelineLayout layout, X&&... params)
{
    extern ZPipeline createGraphicsPipeline (add_ref<GraphicPipelineSettings> settings);
	std::shared_ptr<GraphicPipelineSettings> settings = makeGraphicsPipelineSettings(layout);
	updateSettings(*settings, std::forward<X>(params)...);
	return createGraphicsPipeline(*settings);
}


struct ComputePipelineSettings;
bool computePipelineVerifyLimits (ZDevice device, add_cref<UVec3> wgSizes, bool raise = true);
std::shared_ptr<ComputePipelineSettings> makeComputePipelineSettings();
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, add_cref<ZSpecializationInfo>);
template<class EntryType> void updateKnownSettings (
    add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo> specInfo, add_cref<ZSpecEntry<EntryType>> entry) {
    specInfo.addEntry(entry);
}
#if DESCRIPTOR_HEAP_AVAILABLE
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, add_cref<DescriptorHeapMappings>);
#endif
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, add_cref<UVec3> localSizeValues);
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, add_cref<IVec3> localSizeIndices);
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, ZPipelineLayout);
void updateKnownSettings (add_ref<ComputePipelineSettings>, add_ref<ZSpecializationInfo>, ZPipelineCache);
/**
 * @brief Creates a fully configured Vulkan Compute Pipeline using a type-safe component pack.
 *
 * This function accepts a variable sequence of configuration components in any order.
 * The pipeline internal structures and shader specialization constants are automatically
 * populated based on the types provided.
 *
 * @note **Resource Binding Architecture**:
 * `ZPipelineLayout` and `DescriptorHeapMappings` are mutually interchangeable configurations.
 * Use `ZPipelineLayout` for traditional Descriptor Sets; bindings are managed automatically
 * during pipeline dispatch or on-demand via `commandBufferBindDescriptorSets()`.
 * Use `DescriptorHeapMappings` for modern `VK_EXT_descriptor_heap` (bindless); bindings
 * must be explicitly locked via `commandBufferBindResourceHeap()`.
 *
 * @note **Workgroup Size Configuration**:
 * - `UVec3` (values): Components are mapped to shader specialization constants matching
 * `local_size_{x|y|z}_id` conventions, provided their value is not equal to `INVALID_UINT32`.
 * - `IVec3` (indices): Represents the specialization constant IDs for the X, Y, and Z axes respectively.
 * Components are applied to the corresponding dimensions only if their value is not equal to `-1` (e.g., mapping to indices 0, 1, 2).
 *
 * @note **Specialization Constants Evaluation**:
 * Specialization constants are processed and appended **strictly in the order of their appearance** * in the argument list. Special care must be taken when manually assigning constant IDs
 * to prevent overlapping or conflicting indices, unless they are managed automatically by the framework.
 *
 * @param shaderModule The SPIR-V compute shader stage module to be bound to the pipeline.
 * @param params       A variable sequence of pipeline configuration components.
 * Supported types that are recognized and processed by the engine:
 * - `ZPipelineLayout`          : Traditional pipeline layout defining push constants and descriptor sets.
 * - `DescriptorHeapMappings`   : Bindless descriptor heap layouts and resource mappings (VK_EXT_descriptor_heap).
 * - `UVec3` (localSizeValues)  : Direct 3D local workgroup size dimensions (X, Y, Z).
 * - `IVec3` (localSizeIndices) : Shader specialization indices targeting local workgroup sizes.
 * - `ZSpecializationInfo`      : Full specialization constant data payload.
 * - `ZSpecEntry<EntryType>`    : A single specialization constant entry (e.g., for scalar patching).
 * - `ZPipelineCache`           : Pipeline cache object to accelerate pipeline creation.
 *
 * @return ZPipeline   A fully baked, ready-to-dispatch Vulkan compute pipeline.
 */
template<class... X> ZPipeline createComputePipeline (ZShaderModule shaderModule, X&&... params)
{
    extern ZPipeline createComputePipelineImpl (
        ZShaderModule                    shaderModule,
        add_ref<ComputePipelineSettings> settings,
        add_ref<ZSpecializationInfo>     specInfo);

    ZSpecializationInfo specInfo;
    auto settings = makeComputePipelineSettings();
    (updateKnownSettings(*settings, specInfo, std::forward<X>(params)), ...);

    return createComputePipelineImpl(shaderModule, *settings, specInfo);
}

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline);

ZPipelineCache createPipelineCache (
	ZDevice						device,
	add_cref<std::string>		cacheFileName,
	bool						createEmpty = true,
	bool						saveOnDestroy = true,
	VkPipelineCacheCreateFlags	flags = VkPipelineCacheCreateFlags(0));

} // namespace vtf

#endif // __VTF_ZPIPELINE_HPP_INCLUDED__
