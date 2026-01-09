#include "vtfRTPipeline.hpp"
#include "vtfRTShaderCollection.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfBacktrace.hpp"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <string_view>

namespace vtf
{

// combined hi=normalized logical group index, lo=collection ID
#define COLLECTION_INDEX		0
// combined hi=hitGroupIndex & low=logicalGroupIndex
#define SBT_SHADER_GROUP_INDEX	1
// combined hi=pipelineGroupCoun & lo=pipelineGroupIndex
#define PIPELINE_GROUP_INDEX	2
#define PIPELINE_FIRST_GROUP	3
using tuple4 = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
using tuple3 = std::tuple<uint32_t, uint32_t, uint32_t>;
using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;

extern SBTShaderGroup combineGroups (
	add_cref<SBTShaderGroup> logicalGroup,
	add_cref<SBTShaderGroup> hitGroup);

extern std::pair<SBTShaderGroup, SBTShaderGroup>
decombineGroup (add_cref<SBTShaderGroup> combinedGroup);

void advance (add_ref<uint32_t> val, uint32_t n = 1u)
{
	val = val + n;
}
void retreat (add_ref<uint32_t> val, uint32_t n = 1u)
{
	val = val - n;
}

rtdetails::ShaderCounts countShaders (span::span<const ZShaderModule> shaders)
{
	rtdetails::ShaderCounts counts{};
	for (uint32_t i = 0u; i < shaders.count; ++i)
	{
		switch (auto stage = shaderGetStage(shaders[i]))
		{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:        ++counts.rgCount;	++counts.together; break;
		case VK_SHADER_STAGE_MISS_BIT_KHR:          ++counts.missCount;	++counts.together; break;
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:      ++counts.callCount;	++counts.together; break;
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:       ++counts.ahitCount;	++counts.together; break;
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:   ++counts.chitCount;	++counts.together; break;
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:  ++counts.intCount;	++counts.together; break;
		default: ASSERTFALSE("Improper shader stage ", shaderStageToString(stage));
		}
	}
	return counts;
}

uint32_t shaderGetLogicalGroup (add_cref<ZShaderModule> module)
{
	return decombineGroup(SBTShaderGroup(
		std::get<SBT_SHADER_GROUP_INDEX>(module.getParamRef<tuple4>()))).first.groupIndex();
}

uint32_t shaderGetHitGroup (add_cref<ZShaderModule> module)
{
	return decombineGroup(SBTShaderGroup(
		std::get<SBT_SHADER_GROUP_INDEX>(module.getParamRef<tuple4>()))).second.groupIndex();
}

std::pair<uint32_t, uint32_t> shaderGetPipelineGroup (add_cref<ZShaderModule> module)
{
	const uint32_t val = std::get<PIPELINE_GROUP_INDEX>(module.getParamRef<tuple4>());
	return { (val & 0xFFFF), (val >> 16) };
}

static std::pair<uint32_t, uint32_t> shaderSetPipelineGroup (
	add_ref<ZShaderModule> module, uint32_t group, uint32_t groupCount)
{
	add_ref<uint32_t> field = std::get<PIPELINE_GROUP_INDEX>(module.getParamRef<tuple4>());
	uint32_t old = field;
	field = (group & 0xFFFF) | (groupCount << 16);
	return { (old & 0xFFFF), (old >> 16) };
}

uint32_t shaderGetPipelineFirstGroup (add_cref<ZShaderModule> module)
{
	return std::get<PIPELINE_FIRST_GROUP>(module.getParamRef<tuple4>());
}

static uint32_t shaderSetPipelineFirstGroup (add_ref<ZShaderModule> module, uint32_t group)
{
	add_ref<uint32_t> field = std::get<PIPELINE_FIRST_GROUP>(module.getParamRef<tuple4>());
	uint32_t old = field;
	field = group;
	return old;
}

bool isHitStage (VkShaderStageFlagBits stage)
{
	return VK_SHADER_STAGE_ANY_HIT_BIT_KHR == stage
		|| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR == stage
		|| VK_SHADER_STAGE_INTERSECTION_BIT_KHR == stage;
}

void mapShadersToPipelineGroups (
	span::span<ZShaderModule> shaders,
	span::span<VkRayTracingShaderGroupCreateInfoKHR> groups,
	add_cref<rtdetails::PipelineShaderGroupOrder> order,
	add_ref<std::vector<uint32_t>> hitGroupsMap)
{
	rtdetails::ShaderCounts counts = countShaders(shaders);
	const uint32_t batchGroupCount = counts.batchCount();
	const uint32_t hitGroupCount = counts.hitCount();
	ASSERTION(batchGroupCount <= groups.size());
	ASSERTION(hitGroupCount <= hitGroupsMap.size());
	const bool applyOffsets = false;

	const auto [rgenBaseOffset, missBaseOffset, callBaseOffset, hitBaseOffset] =
		applyOffsets
		? [&]() -> std::array<uint32_t, 4> {
			auto indexToStage = [](const uint32_t index, const uint32_t) -> VkShaderStageFlagBits
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
			};
			const auto orderString = order.toString(); UNREF(orderString);
			rtdetails::PipelineShaderGroupOrder::HitShadersOrder hitShadersOrder{};
			std::array<uint32_t, STED_ARRAY_SIZE(order.getOrder())> ids{ 0, 1, 2, 3 };
			std::array<uint32_t, 4> sizes{ counts.rgCount, counts.missCount, counts.callCount, hitGroupCount };
			order.apply(sizes.begin(), sizes.end(), indexToStage, hitShadersOrder, false);
			order.apply(ids.begin(), ids.end(), indexToStage, hitShadersOrder, false);
			std::array<uint32_t, 4> offsets{
				sizes[ids[0]],
				sizes[ids[0]] + sizes[ids[1]],
				sizes[ids[0]] + sizes[ids[1]] + sizes[ids[2]],
				sizes[ids[0]] + sizes[ids[1]] + sizes[ids[2]] + sizes[ids[3]]
			};

			std::array<uint32_t, 4> offsets2{
				counts.rgCount,
				counts.rgCount + counts.missCount,
				counts.rgCount + counts.missCount + counts.callCount,
				counts.rgCount + counts.missCount + counts.callCount + hitGroupCount
			};

			order.apply(offsets2.begin(), offsets2.end(), indexToStage, hitShadersOrder, false);
			return offsets;
		}()
		: std::array<uint32_t, 4>{ 0u,
								   counts.rgCount,
								   counts.rgCount + counts.missCount,
								   counts.rgCount + counts.missCount + counts.callCount };

	uint32_t rgenVisitCount = 0u;
	uint32_t missVisitCount = 0u;
	uint32_t callVisitCount = 0u;
	uint32_t anyVisitCount = 0u;
	uint32_t closestVisitCount = 0u;
	uint32_t intersectionVisitCount = 0u;

	// sort shaders by their hit subgroup index and ubdate hitGroupMap
	{
		auto shadersRange = makeStdBeginEnd<ZShaderModule>(shaders.data(), shaders.size());
		std::stable_sort(shadersRange.first, shadersRange.second,
			[](add_cref<ZShaderModule> m1, add_cref<ZShaderModule> m2)
			{ return shaderGetHitGroup(m1) < shaderGetHitGroup(m2); });

		anyVisitCount = 0u;
		closestVisitCount = 0u;
		intersectionVisitCount = 0u;
		for (uint32_t i = 0u; i < shaders.size(); ++i)
		{
			const uint32_t hitGroup = shaderGetHitGroup(shaders[i]);
			if (hitGroup) {
				switch (shaderGetStage(shaders[i]))
				{
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: advance(anyVisitCount); break;
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: advance(closestVisitCount); break;
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: advance(intersectionVisitCount); break;
				default: ASSERTFALSE("???");
				}
				const uint32_t hitGroupIndex = std::max(
					{ anyVisitCount, closestVisitCount, intersectionVisitCount }) - 1u;
				ASSERTION(hitGroupIndex < hitGroupCount);
				hitGroupsMap[hitGroupIndex] = hitGroup;
			}
		}
		const auto beginHitGroupsMap = hitGroupsMap.begin();
		const auto endHitGroupsMap = std::next(beginHitGroupsMap, hitGroupCount);
		std::stable_sort(beginHitGroupsMap, endHitGroupsMap);
		ASSERTION(hitGroupCount == std::max({ anyVisitCount, closestVisitCount, intersectionVisitCount }));
		intersectionVisitCount = 0u;
		closestVisitCount = 0u;
		anyVisitCount = 0u;
	}

	for (uint32_t relativeShaderIndex = 0u; relativeShaderIndex < shaders.count; ++relativeShaderIndex)
	{
		add_ref<ZShaderModule> module = shaders[relativeShaderIndex];
		const auto stage = shaderGetStage(module);
		const uint32_t shaderIndex = uint32_t(shaders.start + relativeShaderIndex);

		if (isHitStage(stage))
		{
			const auto beginHitGroupsMap = hitGroupsMap.begin();
			const auto endHitGroupsMap = std::next(beginHitGroupsMap, hitGroupCount);
			const auto foundHitGroupMap = std::find_if(beginHitGroupsMap, endHitGroupsMap,
				[&](add_cref<uint32_t> hitGroupIndex) { return hitGroupIndex == shaderGetHitGroup(module); });
			ASSERTION(foundHitGroupMap != endHitGroupsMap);
			const uint32_t hitGroupOffset = uint32_t(std::distance(beginHitGroupsMap, foundHitGroupMap));
			const uint32_t relativeGroupIndex = hitBaseOffset + hitGroupOffset;
			add_ref<SPAN_TYPE(groups)> group = groups[relativeGroupIndex];
			ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == group.type
						&& VK_SHADER_UNUSED_KHR == group.generalShader);

			if (stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && counts.ahitCount)
			{
				ASSERTION(VK_SHADER_UNUSED_KHR == group.anyHitShader);
				group.anyHitShader = shaderIndex;
				advance(anyVisitCount);
				retreat(counts.ahitCount);
			}

			if (stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR && counts.chitCount)
			{
				ASSERTION(VK_SHADER_UNUSED_KHR == group.closestHitShader);
				group.closestHitShader = shaderIndex;
				advance(closestVisitCount);
				retreat(counts.chitCount);
			}

			if (stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && counts.intCount)
			{
				ASSERTION(VK_SHADER_UNUSED_KHR == group.intersectionShader);
				group.intersectionShader = shaderIndex;
				advance(intersectionVisitCount);
				retreat(counts.intCount);
			}

			shaderSetPipelineGroup(module, relativeGroupIndex, uint32_t(groups.count));
		}
		else
		{
			if (stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR && counts.rgCount)
			{
				const uint32_t relativeGroupIndex = rgenBaseOffset + rgenVisitCount;
				add_ref<SPAN_TYPE(groups)> group = groups[relativeGroupIndex];
				ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == group.type
					&& VK_SHADER_UNUSED_KHR == group.generalShader);
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = shaderIndex;

				shaderSetPipelineGroup(module, relativeGroupIndex, uint32_t(groups.count));
				advance(rgenVisitCount);
				retreat(counts.rgCount);
			}

			if (stage == VK_SHADER_STAGE_MISS_BIT_KHR && counts.missCount)
			{
				const uint32_t relativeGroupIndex = missBaseOffset + missVisitCount;
				add_ref<SPAN_TYPE(groups)> group = groups[relativeGroupIndex];
				ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == group.type
					&& VK_SHADER_UNUSED_KHR == group.generalShader);
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = shaderIndex;

				shaderSetPipelineGroup(module, relativeGroupIndex, uint32_t(groups.count));
				advance(missVisitCount);
				retreat(counts.missCount);
			}

			if (stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR && counts.callCount)
			{
				const uint32_t relativeGroupIndex = callBaseOffset + callVisitCount;
				add_ref<SPAN_TYPE(groups)> group = groups[relativeGroupIndex];
				ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == group.type
					&& VK_SHADER_UNUSED_KHR == group.generalShader);
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = shaderIndex;

				shaderSetPipelineGroup(module, relativeGroupIndex, uint32_t(groups.count));
				advance(callVisitCount);
				retreat(counts.callCount);
			}
		}
	}

	ASSERTMSG(0u == counts.rgCount && 0u == counts.ahitCount
		&& 0u == counts.chitCount && 0u == counts.intCount,
		counts.rgCount, ", ", counts.ahitCount, ", ", counts.chitCount, ", ", counts.intCount);

	auto groupsRange = makeStdBeginEnd<SPAN_TYPE(groups)>(groups.data(), applyOffsets ? 0u : groups.size());
	order.apply(groupsRange.first, groupsRange.second,
		[&](uint32_t, add_cref<SPAN_TYPE(groups)> group) {
			VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			if (group.type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR)
			{
				ASSERTION(VK_SHADER_UNUSED_KHR != group.generalShader);
				stage = shaderGetStage(shaders[group.generalShader - shaders.start]);
			}
			else
			{
				stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			}
			return stage;
		},
		rtdetails::PipelineShaderGroupOrder::HitShadersOrder(), false);

	anyVisitCount = 0u;
	missVisitCount = 0u;
	for (uint32_t relGroupIndex = 0u; relGroupIndex < groups.size(); ++relGroupIndex)
	{
		auto updateShader = [&](const uint32_t shaderIndex, const VkShaderStageFlagBits stage)
			-> bool
		{
			const bool process = shaderIndex != VK_SHADER_UNUSED_KHR;
			if (process)
			{
				add_ref<ZShaderModule> module = shaders[shaderIndex - shaders.start];
				if (isHitStage(stage))
				{
					ASSERTION(stage == shaderGetStage(module));
				}
				else
				{
					ASSERTION(false == isHitStage(shaderGetStage(module)));
				}
				const uint32_t groupIndex = relGroupIndex + groups.start;
				shaderSetPipelineGroup(module, groupIndex, uint32_t(groups.count));
				shaderSetPipelineFirstGroup(module, groups.start);
			}
			return process;
		};
		add_ref<SPAN_TYPE(groups)> group = groups[relGroupIndex];
		updateShader(group.generalShader, VK_SHADER_STAGE_MISS_BIT_KHR);
		updateShader(group.anyHitShader, VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		updateShader(group.closestHitShader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		updateShader(group.intersectionShader, VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
		if (VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR == group.type)
		{
			group.type = VK_SHADER_UNUSED_KHR == group.intersectionShader
				? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
				: VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
			advance(anyVisitCount);
		}
		else
		{
			ASSERTION(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR == group.type);
			advance(missVisitCount);
		}
	}

	ASSERTION(batchGroupCount - hitGroupCount == missVisitCount);
	ASSERTION(hitGroupCount == anyVisitCount);
}

rtdetails::PipelineShaderGroupOrder pipelineGetOrder (ZPipeline rtPipeline)
{
	return rtdetails::PipelineShaderGroupOrder(
		rtPipeline.getParam<ZDistType<LayoutIdentifier, uint32_t>>());
}

std::pair<uint32_t, rtdetails::ShaderCounts>
pipelineGetGroupsInfo (
	ZPipeline rtPipeline,
	add_cref<RTShaderCollection::SBTShaderGroup> group)
{
	uint32_t shaderGroupSize = 0u;
	uint32_t firstShaderIndex = INVALID_UINT32;
	uint32_t pipelineFirstGroupIndex = INVALID_UINT32;
	std::vector<ZShaderModule>::const_iterator firstShaderIter;
	add_cref<std::vector<ZShaderModule>> shaders = rtPipeline.getParamRef<std::vector<ZShaderModule>>();

	for (auto i = shaders.begin(); i != shaders.end(); ++i) {
		// looks for raygen shader
		const bool inGroup = group == SBTShaderGroup(shaderGetLogicalGroup(*i));
		if (inGroup) {
			if (INVALID_UINT32 == firstShaderIndex) {
				firstShaderIndex = uint32_t(std::distance(shaders.begin(), i));
				firstShaderIter = i;
			}
			if (INVALID_UINT32 == pipelineFirstGroupIndex) {
				pipelineFirstGroupIndex = shaderGetPipelineFirstGroup(*i);
			}
			advance(shaderGroupSize);
		}
		else if (INVALID_UINT32 != firstShaderIndex) {
			break;
		}
	}
	const uint32_t groupIndex = decombineGroup(group).first.groupIndex();
	ASSERTMSG((firstShaderIndex != INVALID_UINT32) && (shaderGroupSize != 0u),
		"Unable to find any shader in shader group ", groupIndex);
	const rtdetails::ShaderCounts counts = countShaders(span::make_span(
		shaders, uint32_t(std::distance(shaders.begin(), firstShaderIter)), shaderGroupSize));
	ASSERTMSG(shaderGroupSize == counts.together,
		"Mismatch shader count in group ", groupIndex);
	ASSERTMSG(shaderGetPipelineGroup(*firstShaderIter).second == counts.batchCount(),
		"Mismatch pipeline shader group ", groupIndex);

	return { pipelineFirstGroupIndex, counts };
}

} // namespace vtf

void printGroupCreateInfoTable (
	add_ref<std::ostream> str,
	ZPipeline pipeline,
	add_cptr<VkRayTracingShaderGroupCreateInfoKHR> pGroups,
	const uint32_t groupCount,
	add_cptr<VkPipelineShaderStageCreateInfo> pStages,
	const uint32_t stageCount,
	add_cref<std::vector<ZShaderModule>> shaders)
{
	add_cptr<char> nl = "\n";

	str << "[INFO] const VkPipelineShaderStageCreateInfo[" << stageCount << ']' << nl;
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
	auto testPipelineGroupIndex = [&](uint32_t groupIndex, uint32_t shader) -> std::string {
		UNREF(groupIndex);
		std::ostringstream os;
		if (VK_SHADER_UNUSED_KHR != shader) {
			if (shader >= shaders.size())
				os << "(shader " << shader << " out of bounds " << shaders.size();
			else
			{
				add_cref<ZShaderModule> module = shaders[shader];
				os << "logical group: " << vtf::shaderGetLogicalGroup(module);
				if (vtf::isHitStage(vtf::shaderGetStage(module)))
					os << ", subgroup: " << vtf::shaderGetHitGroup(module);
			}
		}
		os.flush();
		return os.str();
	};
	str << "[INFO] Used pipeline shader group order rule "
		<< pipelineGetOrder(pipeline).toString() << nl;
	str << "[INFO] const VkRayTracingShaderGroupCreateInfoKHR[" << groupCount << "] {" << nl;
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

constexpr size_t pipelineShaderGroupOrderCount = sted_array_size<PipelineShaderGroupOrder::Order>;

const std::pair<VkShaderStageFlagBits, add_cptr<char>> shaderToText[pipelineShaderGroupOrderCount] {
	{ VK_SHADER_STAGE_RAYGEN_BIT_KHR, "R" },
	{ VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "H" },
	// { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "CH" },
	// { VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "IH" },
	{ VK_SHADER_STAGE_MISS_BIT_KHR, "M" },
	{ VK_SHADER_STAGE_CALLABLE_BIT_KHR, "C" }
};
auto findSTTbyStage (VkShaderStageFlagBits stage)
{
	return std::find_if(std::begin(shaderToText), std::end(shaderToText),
		[&](add_cref<std::pair<VkShaderStageFlagBits, add_cptr<char>>> stt)
		{ return stt.first == stage; });
}
auto findSTTbyText (std::string_view text)
{
	return std::find_if(std::begin(shaderToText), std::end(shaderToText),
		[&](add_cref<std::pair<VkShaderStageFlagBits, add_cptr<char>>> stt)
		{ return std::string_view(stt.second) == text; });
}
/*
PipelineShaderGroupOrder::PipelineShaderGroupOrder(add_cref<PipelineShaderGroupOrder> other)
	: m_order(other.m_order)
{
}
*/
PipelineShaderGroupOrder::PipelineShaderGroupOrder (add_cref<std::string> list)
	: PipelineShaderGroupOrder(fromString(list))
{
}
PipelineShaderGroupOrder::PipelineShaderGroupOrder (add_cref<Order> order)
	: m_order(order)
{
	std::set<VkShaderStageFlagBits> s;
	for (uint32_t i = 0; i < pipelineShaderGroupOrderCount; ++i)
	{
		ASSERTMSG(std::end(shaderToText) != findSTTbyStage(order[i]),
			demangledName(*this), "::", __func__, "(...) "
			"Invalid shader stage (", shaderStageToString(order[i]), ") at ", i);
		ASSERTMSG(s.end() == s.find(order[i]),
			demangledName(*this), "::", __func__, "(...) "
			"Multiple shader group definition (", shaderStageToString(order[i]), ')');
		s.insert(order[i]);
	}
}
PipelineShaderGroupOrder::PipelineShaderGroupOrder (uint32_t order)
	: PipelineShaderGroupOrder(fromInt(order))
{
}
PipelineShaderGroupOrder::PipelineShaderGroupOrder (std::function<VkShaderStageFlagBits(uint32_t)> fn)
	: PipelineShaderGroupOrder(fn ? makeOrder(fn) : Default)
{
}

PipelineShaderGroupOrder::Order PipelineShaderGroupOrder::makeOrder (
	std::function<VkShaderStageFlagBits(uint32_t)> fn)
{
	Order order{};
	for (uint32_t i = 0u; i < STED_ARRAY_SIZE(order); ++i)
		order[i] = fn(i);
	return order;
}

bool PipelineShaderGroupOrder::operator== (add_cref<PipelineShaderGroupOrder> other) const
{
	return m_order == other.m_order;
}
bool PipelineShaderGroupOrder::operator!= (add_cref<PipelineShaderGroupOrder> other) const
{
	return !(*this == other);
}
std::string PipelineShaderGroupOrder::toString () const
{
	std::ostringstream str;
	for (uint32_t i = 0; i < pipelineShaderGroupOrderCount; ++i)
	{
		if (i) str << ',';
		str << findSTTbyStage(m_order[i])->second;
	}
	str.flush();
	return str.str();
}
PipelineShaderGroupOrder PipelineShaderGroupOrder::fromString (
	add_cref<std::string> list,
	add_ptr<bool> raiseException)
{
	Order order;
	strings ss = splitString(list, ',');
	std::string s = std::accumulate(ss.begin(), ss.end(), std::string(),
		[](add_cref<std::string> a, add_cref<std::string> b) { return a + b; });
	toUpper(s);
	bool error = false;
	const std::string_view sv(s);
	for (uint32_t i = 0u; i < pipelineShaderGroupOrderCount; ++i)
	{
		const std::string_view chunk = sv.substr(i, 1u);
		if (auto stt = findSTTbyText(chunk); stt != std::end(shaderToText) && !chunk.empty()) {
			order[i] = stt->first;
		}
		else if (raiseException) {
			error = true;
			*raiseException = false;
			break;
		}
		else {
			ASSERTFALSE("Unable to parse shader group order list from \"", list, "\". "
			"Try format \"", Default.toString(), '\"');
		}
	}
	if (false == error && raiseException)
	{
		*raiseException = true;
	}
	return PipelineShaderGroupOrder(order);
}
uint32_t PipelineShaderGroupOrder::toInt () const
{
	Order order = Default.getOrder();
	if (m_order == order) {
		return 0u;
	}
	uint32_t counter = 1u;
	while (std::next_permutation(order.begin(), order.end()))
	{
		if (m_order == order)
			return counter;
		counter = counter + 1u;
	}
	return INVALID_UINT32;
}
PipelineShaderGroupOrder PipelineShaderGroupOrder::fromInt (
	uint32_t order, add_ptr<bool> raiseException)
{
	Order tmpOrder(Default.getOrder());
	if (0u == order) {
		return PipelineShaderGroupOrder(tmpOrder);
	}
	uint32_t counter = 1u;
	while (std::next_permutation(tmpOrder.begin(), tmpOrder.end()))
	{
		if (counter++ == order)
			return PipelineShaderGroupOrder(tmpOrder);
	}
	if (raiseException)	{
		*raiseException = false;
	}
	else {
		ASSERTFALSE(raiseException, "Unable to parse shader order int from ", order);
	}
	return Default;
}
bool PipelineShaderGroupOrder::checkHitShaders (
	add_cref<HitShadersOrder> hitShaders, add_ref<std::string> msg)
{
	std::set<HitShadersOrder::value_type> s;

	for (uint32_t i = 0u; i < STED_ARRAY_SIZE(hitShaders); ++i)
	{
		if (!(hitShaders[i] == VK_SHADER_STAGE_ANY_HIT_BIT_KHR
			|| hitShaders[i] == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
			|| hitShaders[i] == VK_SHADER_STAGE_INTERSECTION_BIT_KHR))
		{
			std::ostringstream os;
			os << "Improper shader stage (" << shaderStageToString(hitShaders[i]) << ". "
				"Only " << shaderStageToString(VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
				<< ", " << shaderStageToString(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
				<< " and " << shaderStageToString(VK_SHADER_STAGE_INTERSECTION_BIT_KHR)
				<< " might be on hitShaders list.";
			os.flush();
			msg = os.str();
			return false;
		}
		if (s.end() != s.find(hitShaders[i]))
		{
			std::ostringstream os;
			os << "Multiple " << shaderStageToString(hitShaders[i]) << " stage on hitSHaders list";
			os.flush();
			msg = os.str();
			return false;
		}
		s.insert(hitShaders[i]);
	}
	return true;
}
PipelineShaderGroupOrder::HitShadersOrder
PipelineShaderGroupOrder::makeHitShadersOrder ()
{
	return HitShadersOrder{
		VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
		VK_SHADER_STAGE_INTERSECTION_BIT_KHR
	};
}
PipelineShaderGroupOrder::ExtendedOrder
PipelineShaderGroupOrder::makeExtendedOrder (
	add_cref<HitShadersOrder> hitShaders, bool considerHitShaders) const
{
	std::string msg;
	ASSERTMSG((false == considerHitShaders) || checkHitShaders(hitShaders, msg), msg);

	uint32_t hitOffset = 0u;
	ExtendedOrder extendedOrder{};
	for (uint32_t i = 0u; i < pipelineShaderGroupOrderCount; ++i)
	{
		if (considerHitShaders && m_order[i] == VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
		{
			extendedOrder[i + hitOffset] = hitShaders[hitOffset]; ++hitOffset;
			extendedOrder[i + hitOffset] = hitShaders[hitOffset]; ++hitOffset;
			extendedOrder[i + hitOffset] = hitShaders[hitOffset];
		}
		else
		{
			extendedOrder[i + hitOffset] = m_order[i];
		}
	}
	if (false == considerHitShaders)
	{
		for (uint32_t i = pipelineShaderGroupOrderCount; i < STED_ARRAY_SIZE(extendedOrder); ++i)
			extendedOrder[i] = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
	return extendedOrder;
}

PipelineShaderGroupOrder PipelineShaderGroupOrder::Default(Order{
	VK_SHADER_STAGE_RAYGEN_BIT_KHR,
	VK_SHADER_STAGE_ANY_HIT_BIT_KHR, // VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
	VK_SHADER_STAGE_MISS_BIT_KHR,
	VK_SHADER_STAGE_CALLABLE_BIT_KHR });

struct RTPipelineSettings
{
	VkPipelineCreateFlags	flags;
	const VkPipelineLibraryCreateInfoKHR*				pLibraryInfo;
	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryInterface;
	const VkPipelineDynamicStateCreateInfo* pDynamicState;
	VkPipeline	basePipelineHandle;
	int32_t		basePipelineIndex;
	uint32_t	maxPipelineRayRecursionDepth;
	PipelineShaderGroupOrder shaderOrder;
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

std::shared_ptr<RTPipelineSettings> makeRTPipelineSettings ()
{
	return std::make_unique<RTPipelineSettings>();
}

void updateKnownSettings (add_ref<RTPipelineSettings> settings, add_cref<PipelineShaderGroupOrder> order) {
	settings.shaderOrder = order;
}
void updateSettings (add_ref<RTPipelineSettings>) {}

ZPipeline createRayTracingPipeline (
	ZPipelineLayout layout,
	add_cref<std::vector<ZShaderModule>> rtShaders,
	add_cptr<RTPipelineSettings> pSettings)
{
	ASSERTMSG(rtShaders.size(), "Shader list must not be empty");
	const uint32_t collectionNum = decombineGroup(SBTShaderGroup(
		std::get<COLLECTION_INDEX>(rtShaders[0].getParam<tuple4>()))).first.groupIndex();
	add_cref<PipelineShaderGroupOrder> pipelineShaderGroupOrder = pSettings->shaderOrder;

	ZDevice device = layout.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	auto callbacks = layout.getParam<VkAllocationCallbacksPtr>();
	add_ref<ProgressRecorder> progressRecorder =
		device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<ProgressRecorder>();
	using SBTShaderGroup = RTShaderCollection::SBTShaderGroup;

	ZPipeline pipeline(VK_NULL_HANDLE, device, callbacks, layout,
		{/*renderpass*/ }, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, VkPipelineCreateFlags(0),
		rtShaders, pipelineShaderGroupOrder.toInt());
	add_ref<std::vector<ZShaderModule>> pipelineShaders = pipeline.getParamRef<std::vector<ZShaderModule>>();
	std::vector<VkPipelineShaderStageCreateInfo> pipelineStages(pipelineShaders.size());

	progressRecorder.stamp("Before create ray-tracing pipeline");

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
			const uint32_t groupShaderCount = std::get<1>(result[groupIndex]);
			const ShaderCounts counts = countShaders(span::make_span(pipelineShaders, shaderIndex, groupShaderCount));
			ASSERTION(groupShaderCount == counts.together);
			// { ..., ..., batchGroupCount, hitGroupCount }
			std::get<2>(result[groupIndex]) = counts.batchCount();
			std::get<3>(result[groupIndex]) = counts.hitCount();
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

		mapShadersToPipelineGroups(span::make_span(pipelineShaders, pipelineShaderIndex, groupShaderCount),
									span::make_span(pipelineGroups, pipelineGroupIndex, pipelineGroupCount),
									pipelineShaderGroupOrder, hitGroupMap);

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

	progressRecorder.stamp("After create ray-tracing pipeline");
	if (getGlobalAppFlags().verbose)
	{
		printGroupCreateInfoTable(std::cout, pipeline,
			info.pGroups, info.groupCount, info.pStages, info.stageCount,
			pipelineShaders);
	}

	return pipeline;
}

} // namespace rtdetails
