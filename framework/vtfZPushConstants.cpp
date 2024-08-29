#include "vtfZPushConstants.hpp"
#include "vtfCUtils.hpp"

namespace vtf
{

void ZPushConstants::appendRange (VkPushConstantRange range, std::type_index type)
{
	range.offset = m_ranges.empty() ? 0u : ROUNDUP(m_ranges.back().offset + m_ranges.back().size, 4u);
	m_ranges.emplace_back(range, type);
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

} // namespace vtf