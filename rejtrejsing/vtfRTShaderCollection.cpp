#include "vtfRTShaderCollection.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include "vulkan/vulkan_to_string.hpp"
#include "demangle.hpp"

#include <algorithm>
#include <array>
#include <numeric>

namespace vtf
{

#define COLLECTION_INDEX		0
#define SBT_SHADER_GROUP_INDEX	1
#define PIPELINE_GROUP_INDEX	2
#define PIPELINE_SHADER_INDEX	3
using tuple4 = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>;
using tuple3 = std::tuple<uint32_t, uint32_t, uint32_t>;

void checkGeneralStage(VkShaderStageFlagBits stage, bool fromFile)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
	case VK_SHADER_STAGE_MISS_BIT_KHR:
	case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		break;
	default:
		ASSERTFALSE("Unsupported shader stage: ", vk::to_string(static_cast<vk::ShaderStageFlagBits>(stage)),
					" - try overloaded addFrom", (fromFile ? "File" : "Text"),
					"(SBTShaderGroup logicalGroup) method");
	}
}

void checkHitStage(VkShaderStageFlagBits stage, bool fromFile)
{
	switch (stage)
	{
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
	case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
	case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		break;
	default:
		ASSERTFALSE("Unsupported shader stage: ", vk::to_string(static_cast<vk::ShaderStageFlagBits>(stage)),
					" - try overloaded addFrom", (fromFile ? "File" : "Text"),
					"(SBTShaderGroup logicalGroup, SBTShaderGroup hitGroup) method");
	}
}

void checkStage (VkShaderStageFlagBits stage, bool hit, bool fromFile)
{
	(void)(hit ? checkHitStage(stage, fromFile) : checkGeneralStage(stage, fromFile));
}

RTShaderCollection::SBTShaderGroup combineGroups(
	add_cref<RTShaderCollection::SBTShaderGroup> logicalGroup,
	add_cref<RTShaderCollection::SBTShaderGroup> hitGroup)
{
	return RTShaderCollection::SBTShaderGroup(
		(logicalGroup.groupIndex() & 0xFFFF) | ((hitGroup.groupIndex() & 0xFFFF) << 16));
}

std::pair<RTShaderCollection::SBTShaderGroup, RTShaderCollection::SBTShaderGroup>
decombineGroup(add_cref<RTShaderCollection::SBTShaderGroup> combinedGroup)
{
	const uint32_t index = combinedGroup.groupIndex();
	return
	{
		RTShaderCollection::SBTShaderGroup(index & 0XFFFF),
		RTShaderCollection::SBTShaderGroup((index >> 16) & 0xFFFF)
	};
}

bool RTShaderCollection::findShaderKey (
	add_cref<SBTShaderGroup>	logicalGroup,
	add_cref<SBTShaderGroup>	hitGroup,
	VkShaderStageFlagBits	stage,
	add_ref<StageAndIndex>	key) const
{
	key = { VK_SHADER_STAGE_ALL, INVALID_UINT32 };
	auto range = m_shaders.equal_range(combineGroups(logicalGroup, hitGroup));
	for (auto iShader = range.first; iShader != range.second; ++iShader)
	{
		if (stage == iShader->second.key().first)
		{
			key = iShader->second.key();
			break;
		}
	}
	return VK_SHADER_STAGE_ALL != key.first;
}

RTShaderCollection::RTShaderCollection(
	ZDevice device,
	HitGroupsOrder hitGroupsOrder,
	add_cref<std::string> basePath)
	: _GlSpvProgramCollection(device, basePath), m_hitGroupsOrder(hitGroupsOrder)
{
}

void RTShaderCollection::addFromText(
	add_cref<SBTShaderGroup>	logicalGroup,
	VkShaderStageFlagBits	stage,
	add_cref<std::string>	code,
	add_cref<strings>		includePaths,
	add_cref<std::string>	entryName)
{
	checkStage(stage, false, false);

	StageAndIndex key;

	if (stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR
		&& findShaderKey(logicalGroup, SBTShaderGroup(0), stage, key))
	{
		m_stageToCode[key].clear();
		_addFromText(stage, code, includePaths, entryName, &key);
	}
	else
	{
		key = _addFromText(stage, code, includePaths, entryName, nullptr);
		m_shaders.insert({ logicalGroup, makeLink(key) });
	}
}

void RTShaderCollection::addFromText(
	add_cref<SBTShaderGroup>	logicalGroup,
	add_cref<SBTShaderGroup>	hitGroup,
	VkShaderStageFlagBits	stage,
	add_cref<std::string>	code,
	add_cref<strings>		includePaths,
	add_cref<std::string>	entryName)
{
	checkStage(stage, true, true);

	StageAndIndex key;
	const SBTShaderGroup maskedHitGroup(hitGroup.groupIndex() + 1u);

	if (findShaderKey(logicalGroup, maskedHitGroup, stage, key))
	{
		m_stageToCode[key].clear();
		_addFromText(stage, code, includePaths, entryName, &key);
	}
	else
	{
		key = _addFromText(stage, code, includePaths, entryName, nullptr);
		m_shaders.insert({ combineGroups(logicalGroup, maskedHitGroup), makeLink(key) });
	}
}

bool RTShaderCollection::addFromFile(
	add_cref<SBTShaderGroup>	logicalGroup,
	VkShaderStageFlagBits	stage,
	add_cref<std::string>	fileName,
	add_cref<strings>		includePaths,
	add_cref<std::string>	entryName,
	bool					verbose)
{
	checkStage(stage, false, false);

	StageAndIndex key;

	if (VK_SHADER_STAGE_RAYGEN_BIT_KHR == stage
		&& findShaderKey(logicalGroup, SBTShaderGroup(0), stage, key))
	{
		m_stageToCode[key].clear();
		_addFromFile(stage, fileName, includePaths, entryName, verbose, &key);
	}
	else
	{
		key = _addFromFile(stage, fileName, includePaths, entryName, verbose, nullptr);
		m_shaders.insert({ logicalGroup, makeLink(key) });
	}

	return VK_SHADER_STAGE_ALL != key.first;
}

bool RTShaderCollection::addFromFile(
	add_cref<SBTShaderGroup>	logicalGroup,
	add_cref<SBTShaderGroup>	hitGroup,
	VkShaderStageFlagBits	stage,
	add_cref<std::string>	fileName,
	add_cref<strings>		includePaths,
	add_cref<std::string>	entryName,
	bool					verbose)
{
	checkStage(stage, true, true);

	StageAndIndex key;
	const SBTShaderGroup maskedHitGroup(hitGroup.groupIndex() + 1u);

	if (findShaderKey(logicalGroup, maskedHitGroup, stage, key))
	{
		m_stageToCode[key].clear();
		_addFromFile(stage, fileName, includePaths, entryName, verbose, &key);
	}
	else
	{
		key = _addFromFile(stage, fileName, includePaths, entryName, verbose, nullptr);
		m_shaders.insert({ combineGroups(logicalGroup, maskedHitGroup), makeLink(key) });
	}

	return VK_SHADER_STAGE_ALL != key.first;
}

void RTShaderCollection::buildAndVerify (add_cref<Version> vulkanVer, add_cref<Version> spirvVer,
										  bool enableValidation, bool genDisassembly, bool buildAlways,
										  add_cref<std::string> spirvValArgs)
{
	_buildAndVerify(vulkanVer, spirvVer, enableValidation, genDisassembly, buildAlways, spirvValArgs);
}

void RTShaderCollection::buildAndVerify (bool buildAlways)
{
	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();
	_buildAndVerify(gf.vulkanVer, gf.spirvVer, gf.spirvValidate, gf.genSpirvDisassembly, buildAlways);
}

ZShaderModule RTShaderCollection::makeShader (add_cref<StageAndIndex> key) const
{
	const VkShaderStageFlagBits stage = key.first;
	auto bin = m_stageToBinary.find(key);
	ASSERTMSG(bin != m_stageToBinary.end(),
		"Unable to find shader stage: ", shaderStageToString(stage),
		", at index: ", key.second);
	add_cref<std::vector<char>> code = bin->second;
	const auto size = code.size();
	ASSERTMSG(size % 4u == 0u, "Shader raw code size (", size, ") must be aligned to four bytes");
	return createShaderModule(m_device, stage,
		reinterpret_cast<add_cptr<uint32_t>>(code.data()), size_t(size), m_stageToCode.at(key).at(entryName));
}

RTShaderCollection::ShaderLink RTShaderCollection::makeLink(add_cref<StageAndIndex> key) const
{
	ShaderLink link;
	link.stage = key.first;
	link.index = key.second;
	return link;
}

std::vector<ZShaderModule> RTShaderCollection::getAllShaders (add_cref<SBTShaderGroup> group) const
{
	const uint32_t collectionInfo = combineGroups(SBTShaderGroup(collectionID()),
													SBTShaderGroup(uint32_t(m_hitGroupsOrder))).groupIndex();
	auto groupMatches = [&](uint32_t count,
		add_cref<std::remove_reference_t<decltype(m_shaders)>::value_type> module) {
			return group == decombineGroup(module.first).first ? (count + 1u) : count;
		};

	const uint32_t shaderCount = std::accumulate(m_shaders.begin(), m_shaders.end(), 0u, groupMatches);

	ASSERTMSG(shaderCount, "Unknown or empty group (", group.groupIndex(), ")");

	uint32_t shaderIndex = 0u;
	std::vector<ZShaderModule> shaders(shaderCount);

	for (auto i = m_shaders.begin(); i != m_shaders.end(); ++i)
	{
		if (groupMatches(0u, *i) != 0u) continue;

		ZShaderModule shaderModule = makeShader(i->second.key());
		shaderModule.setParam<tuple4>({ collectionInfo, i->first.groupIndex(), INVALID_UINT32, INVALID_UINT32 });
		shaders[shaderIndex++] = shaderModule;
	}

	return shaders;
}

std::vector<ZShaderModule> RTShaderCollection::getAllShaders () const
{
	uint32_t shaderIndex = 0u;
	std::vector<ZShaderModule> shaders(m_shaders.size());

	const uint32_t collectionInfo = combineGroups(SBTShaderGroup(collectionID()),
												SBTShaderGroup(uint32_t(m_hitGroupsOrder))).groupIndex();

	for (auto i = m_shaders.begin(); i != m_shaders.end(); ++i)
	{
		ZShaderModule shaderModule = makeShader(i->second.key());
		shaderModule.setParam<tuple4>({ collectionInfo, i->first.groupIndex(), INVALID_UINT32, INVALID_UINT32 });
		shaders[shaderIndex++] = shaderModule;
	}

	return shaders;
}

} // namespace vtf
