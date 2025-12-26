#ifndef __VTF_RT_PIPELINE_HPP_INCLUDED__
#define __VTF_RT_PIPELINE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include <memory>

namespace rtdetails
{

struct ShaderCounts
{
	uint32_t rgCount;
	uint32_t missCount;
	uint32_t ahitCount;
	uint32_t chitCount;
	uint32_t intCount;
	uint32_t callCount;
	uint32_t together;
};
struct RTPipelineSettings;
std::shared_ptr<RTPipelineSettings> makeRTPipelineSettings();
ZPipeline createRayTracingPipeline(
	ZPipelineLayout layout,
	add_cref<std::vector<ZShaderModule>> rtShaders,
	add_cptr<RTPipelineSettings> pSettings);

} // namespace rtdetails

namespace vtf
{

template<class... X>
ZPipeline createRayTracingPipeline(
	ZPipelineLayout layout,
	add_cref<std::vector<ZShaderModule>> rtShaders,
	X&&... params)
{
	MULTI_UNREF(params...);
	auto settings = rtdetails::makeRTPipelineSettings();
	return rtdetails::createRayTracingPipeline(layout, rtShaders, settings.get());
}

} // namespace vtf

#endif // __VTF_RT_PIPELINE_HPP_INCLUDED__
