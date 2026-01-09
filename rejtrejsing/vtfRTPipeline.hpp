#ifndef __VTF_RT_PIPELINE_HPP_INCLUDED__
#define __VTF_RT_PIPELINE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfRTShaderCollection.hpp"


#include <array>
#include <memory>
#include <numeric>

namespace rtdetails
{
using namespace vtf;

struct ShaderCounts
{
	uint32_t rgCount;
	uint32_t missCount;
	uint32_t ahitCount;
	uint32_t chitCount;
	uint32_t intCount;
	uint32_t callCount;
	uint32_t together;
	uint32_t hitCount() const {	return std::max({ ahitCount, chitCount, intCount }); }
	uint32_t batchCount() const { return rgCount + missCount + callCount + hitCount(); }
};

struct PipelineShaderGroupOrder
{
	using Order = std::array<VkShaderStageFlagBits, 4>;
	// comma-separated or not list of { R,H,M,C }, including ignore case,
	// where R indicates general group with raygen shader, M general with miss,
	// C general with callable and H indicates hit group
	PipelineShaderGroupOrder (std::function<VkShaderStageFlagBits(uint32_t)> = {});
	//PipelineShaderGroupOrder (add_cref<PipelineShaderGroupOrder> other);
	PipelineShaderGroupOrder (add_cref<std::string> list);
	PipelineShaderGroupOrder (add_cref<Order> order);
	PipelineShaderGroupOrder (uint32_t order);
	Order getOrder () const { return m_order; }
	static Order makeOrder (std::function<VkShaderStageFlagBits(uint32_t)>);
	constexpr std::size_t getStageCount () const { return sted_array_size<Order>; }
	std::string toString () const;
	uint32_t toInt () const;
	bool operator== (add_cref<PipelineShaderGroupOrder> other) const;
	bool operator!= (add_cref<PipelineShaderGroupOrder> other) const;
	static PipelineShaderGroupOrder fromString (add_cref<std::string> list, add_ptr<bool> raiseException = nullptr);
	static PipelineShaderGroupOrder fromInt (uint32_t order, add_ptr<bool> raiseException = nullptr);
	static PipelineShaderGroupOrder Default; // R,H,M,C

	using HitShadersOrder = std::array<Order::value_type, 3>;
	using ExtendedOrder = std::array<Order::value_type, sted_array_size<Order> + 2>;
	static HitShadersOrder makeHitShadersOrder ();
	static bool checkHitShaders (add_cref<HitShadersOrder> hitShaders, add_ref<std::string> msg);
	ExtendedOrder makeExtendedOrder (add_cref<HitShadersOrder> hitShaders, bool considerHitShaders) const;

	template<class RandomIter, class GetStageOp>
	/*VkShaderStageFlagBits(uint32_t index, std::iterator_traits<RandomIter>::reference const& value*/
	void apply(
		RandomIter first, RandomIter last, GetStageOp getStage,
		add_cref<HitShadersOrder> hitShaders = {}, bool considerHitShaders = false) const
	{
		const ExtendedOrder target = makeExtendedOrder(hitShaders, considerHitShaders);
		const auto indiceCount = make_unsigned(std::distance(first, last));
		std::vector<uint32_t> indices(indiceCount);
		std::iota(indices.begin(), indices.end(), 0u);

		std::stable_sort(indices.begin(), indices.end(),
			[&](const uint32_t a, const uint32_t b) {
				auto sa = getStage(a, *std::next(first, a));
				auto sb = getStage(b, *std::next(first, b));

				auto ia = std::find(target.begin(), target.end(), sa);
				auto ib = std::find(target.begin(), target.end(), sb);

				auto da = std::distance(target.begin(), ia);
				auto db = std::distance(target.begin(), ib);

				return da < db;
			});

		std::vector<uint32_t> inv(indiceCount);
		for (uint32_t i = 0u; i < indiceCount; ++i)
			inv[indices[i]] = i;

		indices = std::move(inv);

		for (uint32_t i = 0; i < indiceCount; ++i)
		{
			while (i != indices[i]) {
				const uint32_t j = indices[i];
				std::iter_swap(std::next(first, i), std::next(first, j));
				std::swap(indices[i], indices[j]);
			}
		}
	}

	template<class RandomIter>
	void apply(RandomIter first, RandomIter last, add_cref<PipelineShaderGroupOrder> order,
		add_cref<HitShadersOrder> hitShaders = {}, bool considerHitShaders = false) const
	{
		auto indexToStage = [&](uint32_t index, add_cref<typename std::iterator_traits<RandomIter>::value_type>)
		{
			return order.getOrder()[index];
		};
		apply(first, last, indexToStage, hitShaders, considerHitShaders);
	}

private:
	Order m_order;
};

template<class X> constexpr size_t RegionDataSizeImpl =
std::is_void_v<X> ? 0 : sizeof(std::conditional_t<std::is_void_v<X>, std::byte, X>);
template<class X> constexpr size_t RegionDataSize =
std::is_void_v<X> ? std::min<size_t>(0, RegionDataSizeImpl<X>) : std::max<size_t>(RegionDataSizeImpl<X>, 0);

struct RTPipelineSettings;
std::shared_ptr<RTPipelineSettings> makeRTPipelineSettings();

void updateKnownSettings (add_ref<RTPipelineSettings> settings, add_cref<PipelineShaderGroupOrder> order);

// end of template recursion
void updateSettings (add_ref<RTPipelineSettings>);

template<class Y, class... X>
void updateSettings (add_ref<RTPipelineSettings> settings, Y&& param, X&&... params)
{
	updateKnownSettings(settings, std::forward<Y>(param));
	updateSettings(settings, std::forward<X>(params)...);
}

ZPipeline createRayTracingPipeline (
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
	rtdetails::updateSettings(*settings, std::forward<X>(params)...);
	return rtdetails::createRayTracingPipeline(layout, rtShaders, settings.get());
}

rtdetails::PipelineShaderGroupOrder pipelineGetOrder (ZPipeline rtPipeline);
std::pair<uint32_t, rtdetails::ShaderCounts> pipelineGetGroupsInfo (
	ZPipeline rtPipeline, add_cref<RTShaderCollection::SBTShaderGroup> group);

} // namespace vtf

#endif // __VTF_RT_PIPELINE_HPP_INCLUDED__
