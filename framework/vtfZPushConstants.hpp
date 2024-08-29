#ifndef __ZPUSH_CONSTANTS_HPP_INCLUDED__
#define __ZPUSH_CONSTANTS_HPP_INCLUDED__

#include "vtfVkUtils.hpp"

namespace vtf
{

template<class X> struct ZPushRange : VkPushConstantRange
{
	ZPushRange (VkShaderStageFlags stages = VK_SHADER_STAGE_ALL)
		: VkPushConstantRange{ stages, 0u, uint32_t(sizeof(X)) }
		, m_type(typeid(X)) { }
	const std::type_index m_type;
};

struct ZPushConstants
{
	ZPushConstants () = default;

	template<class X, class... Y>
		ZPushConstants (const ZPushRange<X>& range, const ZPushRange<Y>&... others)
			: ZPushConstants(others...)
	{
		appendRanges(range);
	}

	template<class X, class... Y> void addRanges (
		const ZPushRange<X>& range, const ZPushRange<Y>&... others)
	{
		appendRanges(range, others...);
	}

	void updateStageFlags (VkShaderStageFlags newFlags);

	auto empty  () const -> bool { return m_ranges.empty(); }
	auto ranges () const -> std::vector<VkPushConstantRange>;

	void clear	() { m_ranges.clear(); }

protected:
	struct ZPushRangeNC : VkPushConstantRange
	{
		ZPushRangeNC (VkPushConstantRange range, const std::type_index type)
			: VkPushConstantRange(range), m_type(type) {}
		ZPushRangeNC (add_cref<ZPushRangeNC> other)
			: VkPushConstantRange(other), m_type(other.m_type) {}
		add_ref<ZPushRangeNC> operator= (add_cref<ZPushRangeNC> other)
		{
			add_ref<VkPushConstantRange>(*this) = other;
			m_type = other.m_type;
			return *this;
		}
		std::type_index m_type;
	};

	void appendRange	(VkPushConstantRange, std::type_index);
	void appendRanges	() {}
	template<class X, class... Y> void appendRanges (const ZPushRange<X>& range, const ZPushRange<Y>&... others)
	{
		appendRange(range, range.m_type);
		appendRanges(others...);
	}

	std::vector<ZPushRangeNC>	m_ranges;
};

} // namespace vtf

#endif // __ZPUSH_CONSTANTS_HPP_INCLUDED__
