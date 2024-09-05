#include "vtfZPushConstants.hpp"
#include "vtfCUtils.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace vtf
{

void ZPushConstants::appendRange (VkPushConstantRange range, std::type_index type)
{
	range.offset = m_ranges.empty() ? 0u : ROUNDUP(m_ranges.back().offset + m_ranges.back().size, 4u);
	m_ranges.emplace_back(range, type);
}

void ZPushConstants::insertRange (VkPushConstantRange range, std::type_index type)
{
	uint32_t offset = 0u;
	m_ranges.insert(m_ranges.begin(), ZPushRangeNC(range, type));
	for (add_ref<ZPushRangeNC> rnc : m_ranges)
	{
		rnc.offset = offset;
		offset = ROUNDUP(offset + rnc.size, 4u);
	}
}

void ZPushConstants::updateStageFlags (VkShaderStageFlags newFlags)
{
	for (add_ref<ZPushRangeNC> range : m_ranges)
	{
		range.stageFlags = newFlags;
	}
}

std::vector<VkPushConstantRange> ZPushConstants::ranges () const
{
	std::vector<VkPushConstantRange> res(m_ranges.size());
	std::transform(m_ranges.begin(), m_ranges.end(), res.begin(), transform_identity());
	return res;
}

std::vector<type_index_with_default> ZPushConstants::types () const
{
	std::vector<type_index_with_default> res(m_ranges.size());
	std::transform(m_ranges.begin(), m_ranges.end(), res.begin(),
		[](add_cref<ZPushRangeNC> range){ return range.m_type; });
	return res;
}

uint32_t ZPushConstants::size () const
{
	return std::accumulate(m_ranges.cbegin(), m_ranges.cend(),
		0u, [](uint32_t x, add_cref<ZPushRangeNC> range) { return x + ROUNDUP(range.size, 4u); });
}

} // namespace vtf