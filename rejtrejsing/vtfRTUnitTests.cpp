#include "vtfRTTypes.hpp"
#include "vtfZUtils.hpp"
#include "vtfZBuffer.hpp"
#include <algorithm>
#include <iostream>
#include <numeric>

namespace
{
using namespace vtf;
using Stage = VkShaderStageFlagBits;
/*
Stage indexToStage(const size_t index)
{
	switch (index)
	{
	case 0: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	case 1: return VK_SHADER_STAGE_MISS_BIT_KHR;
	case 2: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
	case 3: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	default: break;
	}
	return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}
uint32_t stageToIndex(const Stage stage)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return 0;
	case VK_SHADER_STAGE_MISS_BIT_KHR: return 1;
	case VK_SHADER_STAGE_CALLABLE_BIT_KHR: return 2;
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return 3;
	default: break;
	}
	return INVALID_UINT32;
}
*/
/*
void sortNumbers(
	add_cref<rtdetails::PipelineShaderGroupOrder> outputOrder,
	double (&nums)[4], const std::pair<double, Stage>(&numberToStageMap)[4])
{
	auto indexToStage = [&](const uint32_t index) { return numberToStageMap[index].second; };
	const rtdetails::PipelineShaderGroupOrder::HitShadersOrder hitOrder{};
	uint32_t indices[] { 0, 1, 2, 3 };
	outputOrder.apply(std::begin(indices), std::end(indices), indexToStage, hitOrder, false);
	std::sort(std::begin(nums), std::end(nums), [&](double a, double b) {
		return
			std::distance(std::begin(numberToStageMap),
				std::find_if(std::begin(numberToStageMap), std::end(numberToStageMap),
					[&](add_cref<std::pair<double, Stage>> p) { return p.first == a; }))
			< std::distance(std::begin(numberToStageMap),
				std::find_if(std::begin(numberToStageMap), std::end(numberToStageMap),
					[&](add_cref<std::pair<double, Stage>> p) { return p.first == a; })});
}
*/
bool testNumbers()
{
	const rtdetails::PipelineShaderGroupOrder::HitShadersOrder hitOrder{};
	const std::pair<double, Stage> numberToStageMap[]{
		{ 33.0, VK_SHADER_STAGE_ANY_HIT_BIT_KHR },
		{ 37.0, VK_SHADER_STAGE_CALLABLE_BIT_KHR },
		{ 13.0, VK_SHADER_STAGE_MISS_BIT_KHR },
		{ 17.0, VK_SHADER_STAGE_RAYGEN_BIT_KHR }
	};
	const double list[4]{ numberToStageMap[0].first, numberToStageMap[1].first,
						numberToStageMap[2].first, numberToStageMap[3].first };
	auto indexToStage = [&](const uint32_t index, uint32_t = 0u) { return numberToStageMap[index].second; };
	for (uint32_t generator = 0u; generator < 16u; ++generator)
	{
		rtdetails::PipelineShaderGroupOrder order(generator);
		uint32_t indices[4]{ 0, 1, 2, 3 };
		order.apply(std::begin(indices), std::end(indices), indexToStage, hitOrder, false);
		const auto co = order.getOrder();
		for (uint32_t k = 0u; k < order.getStageCount(); ++k)
		{
			const Stage s1 = co[k];
			const Stage s2 = indexToStage(indices[k]);
			if (s1 != s2) {
				return false;
			}
			const double d1 = numberToStageMap[indices[k]].first;
			const double d2 = list[indices[k]];
			if (d1 != d2) {
				return false;
			}
		}
	}
	return true;
}

bool testSBT (
	ZPipeline pipeline,
	RTShaderCollection::SBTShaderGroup group,
	const rtdetails::PipelineShaderGroupOrder testOrder,
	add_ref<std::string> errorMessages)
{
	UNREF(testOrder);
	UNREF(errorMessages);

	ZDevice device = pipeline.getParam<ZDevice>();
	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR props = [&]() {
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR p{};
		p.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		deviceGetPhysicalProperties2(device.getParam<ZPhysicalDevice>(), &p);
		return p;
		}();
	const uint32_t intsPerHandle = uint32_t(props.shaderGroupHandleSize / sizeof(uint32_t));
	const uint32_t intsPerAlignHandle = uint32_t(ROUNDUP(
			props.shaderGroupHandleSize, props.shaderGroupHandleAlignment) / sizeof(uint32_t)); UNREF(intsPerAlignHandle);
	const uint32_t intsPerRecord = uint32_t(
		ROUNDUP(ROUNDUP(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment),
				props.shaderGroupBaseAlignment)) / sizeof(uint32_t);

	const uint32_t debugBegin = (pipelineGetOrder(pipeline).toInt() + 1u) * (group.groupIndex() + 1) * 13u;
	const auto pipelineOrder = pipelineGetOrder(pipeline);
	const SBTHandles handles(pipeline, group, debugBegin); // TODO signedness debugBegin
	const auto counts = handles.getCounts();
	SBT<> sbt(handles);

	std::vector<uint32_t> testHandles;
	testHandles.reserve(intsPerRecord * counts.batchCount());

	auto indexToStage = [](uint32_t index, auto const& sth) -> VkShaderStageFlagBits
	{
		UNREF(sth);
		switch (index)
		{
		case 0: return VK_SHADER_STAGE_MISS_BIT_KHR;
		case 1: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case 2: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case 3: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		default: return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}
	};
	const rtdetails::PipelineShaderGroupOrder bufferOrder(std::bind(indexToStage, std::placeholders::_1, 0));

	sbt.buildOnce(); // build zero produces regions that point to the single buffer
	auto buffers = sbt.getBuffers(bufferOrder);
	auto sbtBuffer = buffers[2]; // indexToStage(2,_) -> VK_SHADER_STAGE_RAYGEN_BIT_KHR
	auto sbtAddress = bufferGetAddress(sbtBuffer);
	const auto sbtSize = bufferGetSize(sbtBuffer); UNREF(sbtSize);
	std::vector<uint32_t> sbtContent;
	bufferRead<uint32_t>(sbtBuffer, sbtContent);

	constexpr auto orderSize = STED_ARRAY_SIZE(bufferOrder.getOrder());
	uint32_t handleInfos[orderSize]{ counts.missCount, counts.callCount, counts.rgCount, counts.hitCount() };
	add_cptr<VkStridedDeviceAddressRegionKHR> regions[orderSize]{ &sbt.missRegion(), &sbt.callableRegion(), &sbt.raygenRegion(), &sbt.hitRegion() };
	pipelineOrder.apply(std::begin(handleInfos), std::end(handleInfos), indexToStage);
	pipelineOrder.apply(regions, regions + orderSize, indexToStage);

	for (uint32_t i = 0u; i < orderSize; ++i)
	{
		const auto stage = indexToStage(i, 0); UNREF(stage);
		add_cref<VkStridedDeviceAddressRegionKHR> region = *regions[i];
		const auto contentIndex = (region.deviceAddress - sbtAddress) / 4u;
		const auto intsPerStride = region.stride / 4u;
		for (uint32_t j = 0; j < handleInfos[i]; ++j)
		{
			for (uint32_t k = 0; k < intsPerHandle; ++k)
			{
				const uint32_t u = sbtContent[contentIndex + j * intsPerStride + k];
				testHandles.push_back(u);
			}
		}
	}

	const auto c1 = [&] {
		std::vector<uint32_t> tmp(testHandles.size());
		std::iota(tmp.begin(), tmp.end(), debugBegin);
		return tmp == testHandles;
		}();

	return c1;
}

} // unnamed namespace

namespace vtf
{
bool rejtrejsingSelfTest()
{
	return testNumbers();
}
bool rejtrejsingSelfTestSBT (
	ZPipeline pipeline,
	RTShaderCollection::SBTShaderGroup group,
	rtdetails::PipelineShaderGroupOrder testOrder,
	add_ref<std::string> errorMessages)
{
	return testSBT(pipeline, group, testOrder, errorMessages);
}

}

