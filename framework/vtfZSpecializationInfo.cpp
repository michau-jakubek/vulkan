#include "vtfZSpecializationInfo.hpp"

#include <numeric>

namespace vtf
{

void ZSpecializationInfo::insertEntry (VkSpecializationMapEntry newEntry, add_cptr<void> data)
{
	pushEntry(newEntry, data, true);
}

void ZSpecializationInfo::appendEntry (VkSpecializationMapEntry newEntry, add_cptr<void> data)
{
	pushEntry(newEntry, data, false);
}

void ZSpecializationInfo::pushEntry (VkSpecializationMapEntry newEntry, add_cptr<void> data, bool insert)
{
	for (add_cref<VkSpecializationMapEntry> entry : m_entries)
	{
		if (INVALID_UINT32 != newEntry.constantID && entry.constantID == newEntry.constantID)
		{
			ASSERTFALSE("Desired new specialization entry with constantID = ",
				newEntry.constantID, " already exists");
		}
	}

	newEntry.offset = data_count(m_data);
	m_entries.insert(insert ? m_entries.begin() : m_entries.end(), newEntry);

	const auto entrySize = ROUNDUP(newEntry.size, 4);

	if (ROUNDUP((m_data.size() + entrySize), 256u) > ROUNDUP(m_data.size(), 256u))
		m_data.reserve((ROUNDUP(m_data.size(), 256) + 1u) * 256u);

	auto src = makeStdBeginEnd<uint8_t>(data, newEntry.size);
	std::copy(src.first, src.second, std::back_inserter(m_data));

	const auto diffSize = entrySize - newEntry.size;
	std::fill_n(std::back_inserter(m_data), diffSize, uint8_t(0u));

	m_modified = true;
}

VkSpecializationInfo ZSpecializationInfo::operator ()()
{
	if (m_modified)
	{
		m_modified = false;

		std::stable_sort(m_entries.begin(), m_entries.end(),
			[](add_cref<VkSpecializationMapEntry> a, add_cref<VkSpecializationMapEntry> b)
			{ return a.constantID < b.constantID; });

		uint32_t id = 0;

		for (add_ref<VkSpecializationMapEntry> entry : m_entries)
		{
			if (INVALID_UINT32 != entry.constantID)
				id = entry.constantID;
			else entry.constantID = id++;

#if 0
			uint32_t	v{};
			UVec2		v2;
			UVec3		v3;
			UVec4		v4;
			switch (entry.size / 4)
			{
			case 1:
				std::memcpy(&v, &m_data.at(entry.offset), entry.size);
				break;
			case 2:
				std::memcpy(&v2, &m_data.at(entry.offset), entry.size);
				break;
			case 3:
				std::memcpy(&v3, &m_data.at(entry.offset), entry.size);
				break;
			case 4:
				std::memcpy(&v4, &m_data.at(entry.offset), entry.size);
				break;
			}
#endif
		}
	}

	add_ref<VkSpecializationInfo> spec(*this);
	spec.dataSize		= data_count(m_data);
	spec.pData			= data_or_null(m_data);
	spec.mapEntryCount	= data_count(m_entries);
	spec.pMapEntries	= data_or_null(m_entries);

	return spec;
}
	
void ZSpecializationInfo::print (add_ref<std::ostream> stream) const
{
	ZSpecializationInfo::print(stream, this);
}

void ZSpecializationInfo::print (add_ref<std::ostream> stream, add_cptr<VkSpecializationInfo> p)
{
	if (nullptr == p)
	{
		stream << "Cannot print anything, a pointer to VkSpecializationInfo is null\n";
		return;
	}

	stream << "dataSize:      " << p->dataSize << std::endl;
	stream << "mapEntryCount: " << p->mapEntryCount << std::endl;
	if (!(p->pMapEntries != nullptr && p->pData != nullptr))
	{
		stream << "pMapEntries:   nullptr" << std::endl;
		stream << "pData:         nullptr" << std::endl;
	}
	else
	{
		uint64_t x = 0;
		add_cptr<VkSpecializationMapEntry> map =
			reinterpret_cast<add_cptr<VkSpecializationMapEntry>>(p->pMapEntries);
		add_cptr<uint8_t> ppData = reinterpret_cast<add_cptr<uint8_t>>(p->pData);

		for (uint32_t e = 0u; e < p->mapEntryCount; ++e)
		{
			add_cref<VkSpecializationMapEntry> entry = map[e];

			x = 0;
			stream << "pMapEntries[" << e << "].constantID = " << entry.constantID << std::endl;
			stream << "pMapEntries[" << e << "].offset:    = " << entry.offset << std::endl;
			stream << "pMapEntries[" << e << "].size:      = " << entry.size << std::endl;
			if (entry.size <= sizeof(x))
			{
				auto src = makeStdBeginEnd<uint8_t>(&ppData[entry.offset], entry.size);
				auto dst = makeStdBeginEnd<uint8_t>(&x, entry.size);
				std::copy(src.first, src.second, dst.first);
				stream << "pMapEntries[" << e << "] value:     = " << x << std::endl;
			}
			else
			{
				std::cout << "pMapEntries[" << e << "] cannot print a value, entry size is greater than sizeof(uint64_t)\n";
			}
		}
	}
}

void ZSpecializationInfo::clear ()
{
	m_data.clear();
	m_entries.clear();
	m_modified = false;
}

} // namespace vtf

