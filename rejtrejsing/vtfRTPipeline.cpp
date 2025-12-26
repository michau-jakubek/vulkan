#include "vtfRTPipeline.hpp"
#include "vtfRTShaderCollection.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfBacktrace.hpp"

#include <iostream>
#include <numeric>

namespace vtf
{

#define COLLECTION_INDEX		0
// combined hi=hitGroupIndex & low=logicalGroupIndex
#define SBT_SHADER_GROUP_INDEX	1
#define PIPELINE_GROUP_INDEX	2
#define PIPELINE_SHADER_INDEX	3
using tuple4 = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
using tuple3 = std::tuple<uint32_t, uint32_t, uint32_t>;
using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;
using HitGroupsOrder = RTShaderCollection::HitGroupsOrder;

extern SBTShaderGroup combineGroups(
	add_cref<SBTShaderGroup> logicalGroup,
	add_cref<SBTShaderGroup> hitGroup);

extern std::pair<SBTShaderGroup, SBTShaderGroup>
decombineGroup(add_cref<SBTShaderGroup> combinedGroup);

void advance(add_ref<uint32_t> val, uint32_t n = 1u)
{
	val = val + n;
}
void retreat(add_ref<uint32_t> val, uint32_t n = 1u)
{
	val = val - n;
}

std::pair<uint32_t, uint32_t> countShaders(
	add_cref<std::vector<ZShaderModule>::iterator> first,
	uint32_t count,	add_ref<rtdetails::ShaderCounts> counts)
{
	counts = {};

	auto last = std::next(first, count);
	for (auto itShader = first; itShader != last; ++itShader)
	{
		switch (shaderGetStage(*itShader))
		{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:		++counts.rgCount;	++counts.together; break;
		case VK_SHADER_STAGE_MISS_BIT_KHR:		++counts.missCount;	++counts.together; break;
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:		++counts.callCount;	++counts.together; break;
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:		++counts.ahitCount;	++counts.together; break;
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:	++counts.chitCount;	++counts.together; break;
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:	++counts.intCount;	++counts.together; break;
		default: ASSERTFALSE("Improper shader stage ", shaderGetStageString(*itShader));
		}
	}

	const uint32_t hitGroupCount = std::max({ counts.ahitCount, counts.chitCount, counts.intCount });
	const uint32_t batchGroupCount = 0u
		+ counts.rgCount
		+ counts.missCount
		+ hitGroupCount
		+ counts.callCount;

	return { batchGroupCount, hitGroupCount };
}

uint32_t shaderGetLogicalGroup(add_cref<ZShaderModule> module)
{
	return decombineGroup(SBTShaderGroup(
		std::get<SBT_SHADER_GROUP_INDEX>(module.getParamRef<tuple4>()))).first.groupIndex();
}

uint32_t shaderGetHitGroup(add_cref<ZShaderModule> module)
{
	return decombineGroup(SBTShaderGroup(
		std::get<SBT_SHADER_GROUP_INDEX>(module.getParamRef<tuple4>()))).second.groupIndex();
}

uint32_t shaderGetPipelineGroup(add_cref<ZShaderModule> module)
{
	return std::get<PIPELINE_GROUP_INDEX>(module.getParamRef<tuple4>());
}

void sortShaders(
	add_ref<std::vector<ZShaderModule>> shaders,
	uint32_t start, uint32_t count,
	HitGroupsOrder hitGroupsOrder)
{
	auto begin = std::next(shaders.begin(), start);
	auto end = std::next(begin, count);
	const VkShaderStageFlagBits desiredStagesOrder[]
	{
		VK_SHADER_STAGE_RAYGEN_BIT_KHR,
		VK_SHADER_STAGE_MISS_BIT_KHR,
		VK_SHADER_STAGE_CALLABLE_BIT_KHR,
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_SHADER_STAGE_INTERSECTION_BIT_KHR
	};
	std::stable_sort(begin, end,
		[&](add_cref<ZShaderModule> a, add_cref<ZShaderModule> b) {
			auto sa = shaderGetStage(a);
			auto sb = shaderGetStage(b);

			auto ia = std::find(std::begin(desiredStagesOrder), std::end(desiredStagesOrder), sa);
			auto ib = std::find(std::begin(desiredStagesOrder), std::end(desiredStagesOrder), sb);

			return ia < ib;
		});

	UNREF(hitGroupsOrder);
}

void mapShadersToPipelineGroups(
	add_ref<std::vector<ZShaderModule>> shaders,
	const uint32_t start, const uint32_t count,
	span::span<VkRayTracingShaderGroupCreateInfoKHR> pgs,
	HitGroupsOrder hitGroupsOrder,
	add_ref<std::vector<uint32_t>> hitGroupsMap)
{
	rtdetails::ShaderCounts counts{};
	const auto batchAndHintGroupCount	= countShaders(std::next(shaders.begin(), start), count, counts);
	const uint32_t batchGroupCount		= batchAndHintGroupCount.first;
	const uint32_t hitGroupCount		= batchAndHintGroupCount.second;
	const uint32_t generalGroupCount	= batchGroupCount - hitGroupCount;
	ASSERTION(batchGroupCount <= pgs.size());
	ASSERTION(hitGroupCount <= hitGroupsMap.size());

	uint32_t generalVisitIndex		= 0u;
	uint32_t anyVisitIndex			= 0u;
	uint32_t closestVisitIndex		= 0u;
	uint32_t intersectionVisitIndex	= 0u;

	for (uint32_t iShader = 0u; (iShader < count) && (generalVisitIndex < generalGroupCount); ++iShader)
	{
		const uint32_t shaderIndex = start + iShader;
		const auto stage = shaderGetStage(shaders[shaderIndex]);

		ASSERTMSG((stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
					|| (stage == VK_SHADER_STAGE_MISS_BIT_KHR)
					|| (stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR),
			"Improper shader stage ", shaderStageToString(stage));
		ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == pgs[generalVisitIndex].type);

		pgs[generalVisitIndex].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		pgs[generalVisitIndex].generalShader = shaderIndex;
		std::get<PIPELINE_GROUP_INDEX>(shaders[shaderIndex].getParamRef<tuple4>()) =
			pgs.start + generalVisitIndex;
		advance(generalVisitIndex);
	}

	if (HitGroupsOrder::InsertOrder == hitGroupsOrder)
	{
		ASSERTMSG(HitGroupsOrder::InsertOrder != hitGroupsOrder,
			"HitGroupsOrder::InsertOrder not supported yet");

		for (uint32_t iShader = 0u; iShader < count; ++iShader)
		{
			const uint32_t shaderIndex = start + iShader;
			const auto stage = shaderGetStage(shaders[shaderIndex]);

			if (stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && counts.ahitCount)
			{
				const uint32_t idx = generalGroupCount + anyVisitIndex;
				pgs[idx].anyHitShader = shaderIndex;
				advance(anyVisitIndex);
				retreat(counts.ahitCount);
			}

			if (stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR && counts.chitCount)
			{
				const uint32_t idx = generalGroupCount + closestVisitIndex;
				pgs[idx].closestHitShader = shaderIndex;
				advance(closestVisitIndex);
				retreat(counts.chitCount);
			}

			if (stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && counts.intCount)
			{
				const uint32_t idx = generalGroupCount + intersectionVisitIndex;
				pgs[idx].intersectionShader = shaderIndex;
				advance(generalVisitIndex);
				retreat(counts.intCount);
			}
		}
	}
	else if (HitGroupsOrder::NumberOrder == hitGroupsOrder)
	{
		uint32_t hitGroupIndex = 0;

		auto begin = hitGroupsMap.begin();

		{
			for (uint32_t iShader = generalGroupCount; iShader < count; ++iShader){
				const uint32_t shaderIndex = start + iShader;
				const uint32_t hitGroupNum = shaderGetHitGroup(shaders[shaderIndex]);

				auto end = std::next(begin, hitGroupIndex);
				auto found = std::find_if(begin, end,
					[&](add_cref<uint32_t> num) {
						return num == hitGroupNum;
					});
				if (found == end) {
					hitGroupsMap[hitGroupIndex] = { hitGroupNum };
					advance(hitGroupIndex);
				}
			}
			ASSERTION(hitGroupCount == hitGroupIndex);
			std::sort(begin, std::next(begin, hitGroupIndex),
				[](add_cref<uint32_t> a, add_cref<uint32_t> b) { return a < b; });
		}

		for (uint32_t iShader = generalGroupCount; iShader < count; ++iShader) {
			const uint32_t shaderIndex = start + iShader;
			const auto stage = shaderGetStage(shaders[shaderIndex]);
			const uint32_t hitGroupOffset = uint32_t(std::distance(
				begin, std::find_if(begin, std::next(begin, hitGroupCount),
					[&](add_cref<uint32_t> g) { return g == shaderGetHitGroup(shaders[shaderIndex]); })));

			ASSERTMSG(stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR
					|| stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
					|| stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
				"Improper shader stage ", shaderStageToString(stage));

			hitGroupIndex = generalGroupCount + hitGroupOffset;

			if (stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && counts.ahitCount) {
				ASSERTMSG(VK_SHADER_UNUSED_KHR == pgs[hitGroupIndex].anyHitShader, "group ", hitGroupIndex);
				pgs[hitGroupIndex].anyHitShader = shaderIndex;
				retreat(counts.ahitCount);
			}
			if (stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR && counts.chitCount) {
				ASSERTMSG(VK_SHADER_UNUSED_KHR == pgs[hitGroupIndex].closestHitShader, "group ", hitGroupIndex);
				pgs[hitGroupIndex].closestHitShader = shaderIndex;
				retreat(counts.chitCount);
			}
			if (stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && counts.intCount) {
				ASSERTMSG(VK_SHADER_UNUSED_KHR == pgs[hitGroupIndex].intersectionShader, "group ", hitGroupIndex);
				pgs[hitGroupIndex].intersectionShader = shaderIndex;
				retreat(counts.intCount);
			}

			std::get<PIPELINE_GROUP_INDEX>(shaders[shaderIndex].getParamRef<tuple4>()) =
				pgs.start + hitGroupIndex;
		}
	}
	else
	{
		ASSERTFALSE("Unknown ordering: ", uint16_t(hitGroupsOrder));
	}

	for (uint32_t hitGroupIndex = generalGroupCount; hitGroupIndex < batchGroupCount; ++hitGroupIndex)
	{
		add_ref<VkRayTracingShaderGroupCreateInfoKHR> hitGroup = pgs[hitGroupIndex];
		ASSERTION(hitGroup.type == VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR);
		hitGroup.type = VK_SHADER_UNUSED_KHR == hitGroup.intersectionShader
			? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
			: VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	}
}

} // namespace vtf

void printGroupCreateInfoTable(
	add_ref<std::ostream> str,
	add_cptr<VkRayTracingShaderGroupCreateInfoKHR> pGroups,
	const uint32_t groupCount,
	add_cptr<VkPipelineShaderStageCreateInfo> pStages,
	const uint32_t stageCount,
	add_cref<std::vector<ZShaderModule>> shaders)
{
	add_cptr<char> nl = "\n";

	str << "const VkPipelineShaderStageCreateInfo[" << stageCount << ']' << nl;
	for (uint32_t i = 0u; i < stageCount; ++i)
	{
		const auto stage1 = pStages[i].stage;
		const auto stage2 = vtf::shaderGetStage(shaders[i]);
		str << i << ':' << vtf::shaderStageToString(stage1);
		if (stage1 != stage2)
		{
			str << '(' << vtf::shaderStageToString(stage2) << ')';
		}
		str << ' ';
	}
	str << nl;

	add_cptr<char> ind4 = "    ";
	const std::string unusedShader = "VK_SHADER_UNUSED_KHR";

	// mode == true: shaderStage comes from pipeline group
	// mode == false: shaderStage comes from _shaders_ vector
	auto testShaderInGroup = [&](
		uint32_t groupIndex, VkRayTracingShaderGroupTypeKHR groupType,
		uint32_t shaderType, uint32_t shaderValue, VkShaderStageFlagBits shaderStage, bool mode) -> char
		{
			// RG(0), CH(1), AH(2), IH(3)
			UNREF(groupIndex);
			UNREF(shaderValue);
			bool ok = false;
			if (VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR == groupType)
			{
				switch (shaderType)
				{
				case 0: ok = VK_SHADER_STAGE_RAYGEN_BIT_KHR == shaderStage
					|| VK_SHADER_STAGE_MISS_BIT_KHR == shaderStage
					|| VK_SHADER_STAGE_CALLABLE_BIT_KHR == shaderStage ; break;
				default: ok = VK_SHADER_STAGE_ALL == shaderStage; break;
				};
			}
			else if (VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR == groupType)
			{
				switch (shaderType)
				{
				case 0: ok = VK_SHADER_STAGE_ALL == shaderStage; break;
				case 1: ok = VK_SHADER_STAGE_ALL == shaderStage || VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR == shaderStage; break;
				case 2: ok = VK_SHADER_STAGE_ALL == shaderStage || VK_SHADER_STAGE_ANY_HIT_BIT_KHR == shaderStage; break;
				case 3: ok = VK_SHADER_STAGE_ALL == shaderStage; break;
				}
			}
			else { // VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
				switch (shaderType)
				{
				case 0: ok = VK_SHADER_STAGE_ALL == shaderStage; break;
				case 1: ok = VK_SHADER_STAGE_ALL == shaderStage || VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR == shaderStage; break;
				case 2: ok = VK_SHADER_STAGE_ALL == shaderStage || VK_SHADER_STAGE_ANY_HIT_BIT_KHR == shaderStage; break;
				case 3: ok = VK_SHADER_STAGE_INTERSECTION_BIT_KHR == shaderStage; break;
				}
			}

			return ok ? ' ' : mode ? '?' : '*';
		};
	auto testPipelineGroupIndex = [&](uint32_t groupIndex, uint32_t shader) -> char {
		bool ok = true;
		if (VK_SHADER_UNUSED_KHR != shader) {
			if (shader >= shaders.size())
				return 'I';
			ok = std::get<PIPELINE_GROUP_INDEX>(shaders[shader].getParamRef<vtf::tuple4>()) == groupIndex;
		}
		return ok ? ' ' : 'I';
	};
	str << "const VkRayTracingShaderGroupCreateInfoKHR[" << groupCount << "] {" << nl;
	for (uint32_t i = 0u; i < groupCount; ++i)
	{
		add_cref<VkRayTracingShaderGroupCreateInfoKHR> c = pGroups[i];

		const VkShaderStageFlagBits genStage =
			c.generalShader == VK_SHADER_UNUSED_KHR || c.generalShader >= stageCount
			? VK_SHADER_STAGE_ALL : pStages[c.generalShader].stage;
		const VkShaderStageFlagBits zgenStage =
			c.generalShader == VK_SHADER_UNUSED_KHR || c.generalShader >= shaders.size()
			? VK_SHADER_STAGE_ALL : shaderGetStage(shaders[c.generalShader]);

		const VkShaderStageFlagBits chitStage =
			c.closestHitShader == VK_SHADER_UNUSED_KHR || c.closestHitShader >= stageCount
			? VK_SHADER_STAGE_ALL : pStages[c.closestHitShader].stage;
		const VkShaderStageFlagBits zchitStage =
			c.closestHitShader == VK_SHADER_UNUSED_KHR || c.closestHitShader >= shaders.size()
			? VK_SHADER_STAGE_ALL : shaderGetStage(shaders[c.closestHitShader]);

		const VkShaderStageFlagBits ahitStage =
			c.anyHitShader == VK_SHADER_UNUSED_KHR || c.anyHitShader >= stageCount
			? VK_SHADER_STAGE_ALL : pStages[c.anyHitShader].stage;
		const VkShaderStageFlagBits zahitStage =
			c.anyHitShader == VK_SHADER_UNUSED_KHR || c.anyHitShader >= shaders.size()
			? VK_SHADER_STAGE_ALL : shaderGetStage(shaders[c.anyHitShader]);

		const VkShaderStageFlagBits intStage =
			c.intersectionShader == VK_SHADER_UNUSED_KHR || c.intersectionShader >= stageCount
			? VK_SHADER_STAGE_ALL : pStages[c.intersectionShader].stage;
		const VkShaderStageFlagBits zintStage =
			c.intersectionShader == VK_SHADER_UNUSED_KHR || c.intersectionShader >= shaders.size()
			? VK_SHADER_STAGE_ALL : shaderGetStage(shaders[c.intersectionShader]);

		str << "  " << i << ": {" << nl;
		str << ind4 << "type: " << vk::to_string(static_cast<vk::RayTracingShaderGroupTypeKHR>(c.type)) << nl;

		str << ind4 << "generalShader:      " << (genStage == VK_SHADER_STAGE_ALL
			? unusedShader : vtf::shaderStageToString(genStage))
			<< ' ' << testShaderInGroup(i, c.type, 0, c.generalShader, genStage, true)
			<< ' ' << testShaderInGroup(i, c.type, 0, c.generalShader, zgenStage, false)
			<< ' ' << testPipelineGroupIndex(i, c.generalShader) << nl;

		str << ind4 << "closestHitShader:   " << (chitStage == VK_SHADER_STAGE_ALL
			? unusedShader : vtf::shaderStageToString(chitStage))
			<< ' ' << testShaderInGroup(i, c.type, 1, c.closestHitShader, chitStage, true)
			<< ' ' << testShaderInGroup(i, c.type, 1, c.closestHitShader, zchitStage, false)
			<< ' ' << testPipelineGroupIndex(i, c.closestHitShader) << nl;

		str << ind4 << "anyHitShader:       " << (ahitStage == VK_SHADER_STAGE_ALL
			? unusedShader : vtf::shaderStageToString(ahitStage))
			<< ' ' << testShaderInGroup(i, c.type, 2, c.anyHitShader, ahitStage, true)
			<< ' ' << testShaderInGroup(i, c.type, 2, c.anyHitShader, zahitStage, false)
			<< ' ' << testPipelineGroupIndex(i, c.anyHitShader) << nl;

		str << ind4 << "intersectionShader: " << (intStage == VK_SHADER_STAGE_ALL
			? unusedShader : vtf::shaderStageToString(intStage))
			<< ' ' << testShaderInGroup(i, c.type, 3, c.intersectionShader, intStage, true)
			<< ' ' << testShaderInGroup(i, c.type, 3, c.intersectionShader, zintStage, false)
			<< ' ' << testPipelineGroupIndex(i, c.intersectionShader) << nl;

		str << "  }" << nl;
	}
	str << '}' << nl;
}

namespace rtdetails
{
using namespace vtf;

struct RTPipelineSettings
{
	VkPipelineCreateFlags	flags;
	const VkPipelineLibraryCreateInfoKHR*				pLibraryInfo;
	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryInterface;
	const VkPipelineDynamicStateCreateInfo* pDynamicState;
	VkPipeline	basePipelineHandle;
	int32_t		basePipelineIndex;
	uint32_t	maxPipelineRayRecursionDepth;
	RTPipelineSettings()
		: flags(0)
		, pLibraryInfo(nullptr)
		, pLibraryInterface(nullptr)
		, pDynamicState(nullptr)
		, basePipelineHandle(VK_NULL_HANDLE)
		, basePipelineIndex(-1)
		, maxPipelineRayRecursionDepth(1)
	{
	}
};

std::shared_ptr<RTPipelineSettings> makeRTPipelineSettings()
{
	return std::make_unique<RTPipelineSettings>();
}

ZPipeline createRayTracingPipeline(
	ZPipelineLayout layout,
	add_cref<std::vector<ZShaderModule>> rtShaders,
	add_cptr<RTPipelineSettings> pSettings)
{
	ASSERTMSG(rtShaders.size(), "Shader list must not be empty");
	const uint32_t collectionNum = decombineGroup(SBTShaderGroup(
		std::get<COLLECTION_INDEX>(rtShaders[0].getParam<tuple4>()))).first.groupIndex();
	const HitGroupsOrder collectionOrder = HitGroupsOrder(decombineGroup(SBTShaderGroup(
		std::get<COLLECTION_INDEX>(rtShaders[0].getParam<tuple4>()))).second.groupIndex());

	ZDevice device = layout.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	auto callbacks = layout.getParam<VkAllocationCallbacksPtr>();
	add_ref<ProgressRecorder> progressRecorder =
		device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();
	using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;

	ZPipeline pipeline(VK_NULL_HANDLE, device, callbacks, layout,
		{/*renderpass*/ }, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, VkPipelineCreateFlags(0), rtShaders);
	add_ref<std::vector<ZShaderModule>> pipelineShaders = pipeline.getParamRef<std::vector<ZShaderModule>>();
	std::vector<VkPipelineShaderStageCreateInfo> pipelineStages(pipelineShaders.size());

	if (getGlobalAppFlags().verbose) {
		progressRecorder.stamp("Before create ray-tracing pipeline");
	}

	std::stable_sort(pipelineShaders.begin(), pipelineShaders.end(),
		[&](add_cref<ZShaderModule> lhs, add_cref<ZShaderModule> rhs) {
			return shaderGetLogicalGroup(lhs) < shaderGetLogicalGroup(rhs);
		});;

	// { logicalGroup, allShaderCount, batchGroupCount, hitGroupCount }
	const std::vector<tuple4> groupToShaderCount = [&]() {
		std::map<uint32_t, uint32_t> groupToSizes;
		for (add_cref<ZShaderModule> shader : pipelineShaders)
		{
			const uint32_t collectionID = decombineGroup(SBTShaderGroup(
				std::get<COLLECTION_INDEX>(shader.getParam<tuple4>()))).first.groupIndex();
			ASSERTMSG(collectionID == collectionNum, "All shaders must come from the same collection");
			const uint32_t groupIndex = shaderGetLogicalGroup(shader);
			groupToSizes[groupIndex] += 1u;
		}
		std::vector<tuple4> result(groupToSizes.size());
		for (auto begin = groupToSizes.begin(), i = begin; i != groupToSizes.end(); ++i)
		{
			// { logicalGroup, allShaderCount, 0, 0 }
			result[make_unsigned(std::distance(begin, i))] = { i->first, i->second, 0u, 0u };
		}
		std::sort(result.begin(), result.end(),
				[](add_cref<tuple4> a, add_cref<tuple4> b) { return std::get<0>(a) < std::get<0>(b); });
		uint32_t shaderIndex = 0u;
		for (uint32_t groupIndex = 0u; groupIndex < result.size(); ++groupIndex)
		{
			ShaderCounts counts;
			const uint32_t groupShaderCount = std::get<1>(result[groupIndex]);
			const auto batchAndHintGroupCount =
				countShaders(std::next(pipelineShaders.begin(), shaderIndex), groupShaderCount, counts);
			ASSERTION(groupShaderCount == counts.together);
			// { ..., ..., batchGroupCount, hitGroupCount }
			std::get<2>(result[groupIndex]) = batchAndHintGroupCount.first;
			std::get<3>(result[groupIndex]) = batchAndHintGroupCount.second;
			advance(shaderIndex, groupShaderCount);
		}
		ASSERTION(pipelineShaders.size() == shaderIndex);
		return result;
	}();

	uint32_t pipelineGroupIndex = 0u;
	const uint32_t allPipelineGroupCount =
		std::accumulate(groupToShaderCount.begin(), groupToShaderCount.end(),
			0u, [](uint32_t a, add_cref<tuple4> b) { return a + std::get<2>(b); });
	const uint32_t largestHitGroupCount = std::get<3>(
		*std::max_element(groupToShaderCount.begin(), groupToShaderCount.end(),
		[](add_cref<tuple4> a, add_cref<tuple4> b) { return std::get<3>(a) < std::get<3>(b); }));
	const VkRayTracingShaderGroupCreateInfoKHR vkRTSGTemplate
	{
		VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,	// VkStructureType		sType;
		nullptr,										// const void*						pNext;
		VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR,	// VkRayTracingShaderGroupTypeKHR	type;
		VK_SHADER_UNUSED_KHR,	// uint32_t		generalShader;
		VK_SHADER_UNUSED_KHR,	// uint32_t		closestHitShader;
		VK_SHADER_UNUSED_KHR,	// uint32_t		anyHitShader;
		VK_SHADER_UNUSED_KHR,	// uint32_t		intersectionShader;
		nullptr					// const void*	pShaderGroupCaptureReplayHandle;
	};
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> pipelineGroups(allPipelineGroupCount, vkRTSGTemplate);

	uint32_t pipelineShaderIndex = 0u;
	const uint32_t groupIndiceCount = data_count(groupToShaderCount);
	std::vector<uint32_t> hitGroupMap(largestHitGroupCount);

	for (uint32_t indiceGroupIndex = 0; indiceGroupIndex < groupIndiceCount; ++indiceGroupIndex)
	{
		const uint32_t groupShaderCount = std::get<1>(groupToShaderCount[indiceGroupIndex]);
		const uint32_t pipelineGroupCount = std::get<2>(groupToShaderCount[indiceGroupIndex]);
		sortShaders(pipelineShaders, pipelineShaderIndex, groupShaderCount, collectionOrder);

		mapShadersToPipelineGroups(pipelineShaders, pipelineShaderIndex, groupShaderCount,
									span::make_span(pipelineGroups, pipelineGroupIndex, pipelineGroupCount),
									collectionOrder, hitGroupMap);

		advance(pipelineShaderIndex, groupShaderCount);
		advance(pipelineGroupIndex, pipelineGroupCount);
	}
	ASSERTION(pipelineShaders.size() == pipelineShaderIndex);
	ASSERTION(allPipelineGroupCount == pipelineGroupIndex);

	std::transform(pipelineShaders.begin(), pipelineShaders.end(), pipelineStages.begin(),
		[&](add_cref<ZShaderModule> module) -> VkPipelineShaderStageCreateInfo {
			VkPipelineShaderStageCreateInfo stage{};
			stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage.pNext = nullptr;
			stage.module = *module;
			stage.stage = module.getParam<VkShaderStageFlagBits>();
			stage.flags = VkPipelineShaderStageCreateFlags(0);
			stage.pName = module.getParamRef<std::string>().c_str();
			stage.pSpecializationInfo = nullptr;
			return stage;
		});

	VkRayTracingPipelineCreateInfoKHR info = {};
	info.sType		= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	info.stageCount	= data_count(pipelineStages);
	info.pStages	= pipelineStages.data();
	info.groupCount	= pipelineGroupIndex;
	info.pGroups	= pipelineGroups.data();
	info.layout		= *layout;

	info.flags		= pSettings->flags;
	info.pLibraryInfo		= pSettings->pLibraryInfo;
	info.pLibraryInterface	= pSettings->pLibraryInterface;
	info.pDynamicState		= pSettings->pDynamicState;
	info.basePipelineHandle	= pSettings->basePipelineHandle;
	info.basePipelineIndex	= pSettings->basePipelineIndex;
	info.maxPipelineRayRecursionDepth	= pSettings->maxPipelineRayRecursionDepth;

	VKASSERTMSG(di.vkCreateRayTracingPipelinesKHR(*device,
						VK_NULL_HANDLE, VK_NULL_HANDLE, 1u,
						&info, callbacks, pipeline.setter("vkCreateRayTracingPipelinesKHR")),
						"Fail to cretae ray tracing pipeline");

	if (getGlobalAppFlags().verbose)
	{
		progressRecorder.stamp("After create ray-tracing pipeline");
		printGroupCreateInfoTable(std::cout,
			info.pGroups, info.groupCount, info.pStages, info.stageCount,
			pipelineShaders);
	}

	return pipeline;
}

} // namespace rtdetails
