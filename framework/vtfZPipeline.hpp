#ifndef __VTF_ZPIPELINE_HPP_INCLUDED__
#define __VTF_ZPIPELINE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfPipelineLayout.hpp"
#include "vtfVertexInput.hpp"
#include <memory>

namespace vtf
{

struct GraphicPipelineSettings;

namespace gpp // Graphics Pipeline Param
{
	using PatchControlPoints	= ZDistType<PatchControlPoints, uint32_t>;
	using CullModeFlags			= ZDistType<CullModeFlags, VkCullModeFlags>;
	using DepthTestEnable		= ZDistType<DepthTestEnable, bool>;
	using DepthWriteEnable		= ZDistType<DepthWriteEnable, bool>;
	using StencilTestEnable		= ZDistType<StencilTestEnable, bool>;
	using SubpassIndex			= ZDistType<SubpassIndex, uint32_t>;
	using ViewportCount			= ZDistType<ViewportCount, uint32_t>;
	using ScissorCount			= ZDistType<ScissorCount, uint32_t>;
	using LineWidth				= ZDistType<LineWidth, float>;

	// VkExtent2D	sets both viewport and scissor
	// VkViewport	sets viewport only
	// VkRect2D		sets scissor onlu
} // gpp

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

// end of template recursion
void updateSettings (add_ref<GraphicPipelineSettings>);

template<class Y, class... X>
void updateSettings (add_ref<GraphicPipelineSettings> settings, Y&& param, X&&... params)
{
	updateKnownSettings(settings, param);
	updateSettings(settings, std::forward<X>(params)...);
}

std::shared_ptr<GraphicPipelineSettings> makeGraphicsPipelineSettings (ZPipelineLayout layout, ZRenderPass renderPass);

ZPipeline createGraphicsPipeline (add_ref<GraphicPipelineSettings> settings);

template<class... X>
ZPipeline createGraphicsPipeline (ZPipelineLayout layout, ZRenderPass renderPass, X&&... params)
{
	std::shared_ptr<GraphicPipelineSettings> settings = makeGraphicsPipelineSettings(layout, renderPass);
	updateSettings(*settings, std::forward<X>(params)...);
	return createGraphicsPipeline(*settings);
}

// Please note that if any of localSize[?] is valid value and a layout of compute shader looks like
// layout(local_size_x_ID = X, local_size_y_ID = Y, local_size_z_ID = Z), then X,Y,Z will refer to
// the SpecID during compute pipeline creation. Be carefull to set them properly according to their
// index in localSize vector or left as INVALID_UINT32.
ZPipeline createComputePipeline (ZPipelineLayout layout, ZShaderModule computeShaderModule,
								 add_cref<UVec3> workGroupSize = UVec3(INVALID_UINT32),
								 add_cref<UVec3> specID = UVec3(0,1,2), bool enableFullGroups = false);

ZPipelineLayout	pipelineGetLayout (ZPipeline pipeline);

} // namespace vtf

#endif // __VTF_ZPIPELINE_HPP_INCLUDED__
