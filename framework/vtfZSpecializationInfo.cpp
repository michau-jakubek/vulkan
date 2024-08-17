#include "vtfZSpecializationInfo.hpp"

#include <numeric>

namespace vtf
{

void ZSpecializationInfo::appendEntry (VkSpecializationMapEntry newEntry, add_cptr<void> data)
{
	for (add_cref<VkSpecializationMapEntry> entry : m_entries)
	{
		if (INVALID_UINT32 != newEntry.constantID && entry.constantID == newEntry.constantID)
		{
			std::ostringstream os;
			os << "Desired new specialization entry with constantID = ";
			os << newEntry.constantID << " already exists";
			ASSERTMSG(false, os.str());
		}
	}

	newEntry.offset = data_count(m_data);
	m_entries.push_back(newEntry);

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

		std::sort(m_entries.begin(), m_entries.end(),
			[](add_cref<VkSpecializationMapEntry> a, add_cref<VkSpecializationMapEntry> b)
			{ return a.constantID < b.constantID; });

		uint32_t id = 0;

		for (add_ref<VkSpecializationMapEntry> entry : m_entries)
		{
			if (INVALID_UINT32 != entry.constantID)
				id = entry.constantID;
			else entry.constantID = id++;
		}
	}

	add_ref<VkSpecializationInfo> spec(*this);
	spec.dataSize = data_count(m_data);
	spec.pData = data_or_null(m_data);
	spec.mapEntryCount = data_count(m_entries);
	spec.pMapEntries = data_or_null(m_entries);

	return spec;
}
	
void ZSpecializationInfo::print (add_ref<std::ostream> stream) const
{
	stream << "dataSize:      " << dataSize << std::endl;
	stream << "mapEntryCount: " << mapEntryCount << std::endl;
	if (!(pMapEntries != nullptr && pData != nullptr))
	{
		stream << "pMapEntries:   nullptr" << std::endl;
		stream << "pData:         nullptr" << std::endl;
	}
	else
	{
		uint64_t x = 0;
		for (add_cref<VkSpecializationMapEntry> entry : m_entries)
		{
			x = 0;
			stream << "constantID = " << entry.constantID << std::endl;
			stream << "offset:    = " << entry.offset << std::endl;
			stream << "size:      = " << entry.size << std::endl;
			if (entry.size <= sizeof(x))
			{
				auto src = makeStdBeginEnd<uint8_t>(&m_data.data()[entry.offset], entry.size);
				auto dst = makeStdBeginEnd<uint8_t>(&x, entry.size);
				std::copy(src.first, src.second, dst.first);
				stream << "value:     = " << x << std::endl;
			}
			else
			{
				std::cout << "Entry size is greater than sizeof(uint64_t)\n";
			}
		}
	}
}

} // namespace vtf

