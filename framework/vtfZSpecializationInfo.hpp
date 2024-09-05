#ifndef __VTF_ZSPECIALIZATION_INFO_HPP_INCLUDED__
#define __VTF_ZSPECIALIZATION_INFO_HPP_INCLUDED__

#include "vtfVkUtils.hpp"

namespace vtf
{

template<class X> struct ZSpecEntry : VkSpecializationMapEntry
{
	ZSpecEntry (add_cref<X> data, uint32_t id = INVALID_UINT32)
		: VkSpecializationMapEntry{ id, 0u, sizeof(X) }, m_type(typeid(X)), m_data(data) {}

	const std::type_index	m_type;
	X						m_data;
};

struct ZSpecializationInfo : protected VkSpecializationInfo
{
	ZSpecializationInfo ()
		: VkSpecializationInfo	{}
		, m_modified			(false)
		, m_data				()
		, m_entries				()
	{
	}

	template<class X, class... Y>
		ZSpecializationInfo (const ZSpecEntry<X>& entry, const ZSpecEntry<Y>&... entries)
			: ZSpecializationInfo(entries...)
	{
		insertEntry(entry, &entry.m_data);
	}

	template<class X> void addEntry (add_cref<X> data, uint32_t id = INVALID_UINT32)
	{
		appendEntry(ZSpecEntry<X>(data, id), &data);
	}
	template<class... X> void addEntries (const ZSpecEntry<X>&... entries)
	{
		appendEntries(entries...);
	}

	uint32_t	entryCount	() const { return data_count(m_entries); }
	bool		empty		() const { return m_entries.empty(); }
	void		clear		();

	VkSpecializationInfo operator ()();

	static	void print (add_ref<std::ostream> stream, add_cptr<VkSpecializationInfo> p);
			void print (add_ref<std::ostream> stream) const;

protected:
	void pushEntry		(VkSpecializationMapEntry newEntry, add_cptr<void> data, bool);
	void appendEntry	(VkSpecializationMapEntry newEntry, add_cptr<void> data);
	void insertEntry	(VkSpecializationMapEntry newEntry, add_cptr<void> data);
	void appendEntries	()	{ }
	template<class X, class... Y> void appendEntries (const ZSpecEntry<X>& entry, const ZSpecEntry<Y>&... others)
	{
		appendEntry(entry, &entry.m_data);
		appendEntries(others...);
	}

private:
	bool									m_modified;
	std::vector<uint8_t>					m_data;
	std::vector<VkSpecializationMapEntry>	m_entries;
};

} // namespace vtf

#endif // __VTF_ZSPECIALIZATION_INFO_HPP_INCLUDED__