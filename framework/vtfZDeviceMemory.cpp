#include "vtfZDeviceMemory.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZBuffer.hpp"

namespace vtf
{

uint32_t findMemoryTypeIndex (ZDevice device, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
{
	auto physicalDevice = device.getParam<ZPhysicalDevice>();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(*physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

std::vector<ZDeviceMemory> createMemory (ZDevice device, add_cref<VkMemoryRequirements> requirements,
										VkMemoryPropertyFlags properties, VkDeviceSize desiredSize,
										bool sparse, bool deviceAddress)
{
	auto						callbacks		= device.getParam<VkAllocationCallbacksPtr>();
	const uint32_t				chunkCount		= (uint32_t)(sparse ? ROUNDUP(desiredSize, requirements.alignment) / requirements.alignment : 1u);
	const VkDeviceSize			allocationSize = sparse ? requirements.alignment : ROUNDUP(requirements.size, requirements.alignment);
	const uint32_t				memoryTypeIndex = findMemoryTypeIndex(device, requirements.memoryTypeBits, properties);
	//add_ptr<void>				pNext			{ /* VkExternalMemoryBufferCreateInfo, VkExternalMemoryImageCreateInfo */ };
	std::vector<ZDeviceMemory>	allocations		(chunkCount);

	VkExternalMemoryImageCreateInfo ici{};
	VkExternalMemoryBufferCreateInfo ibi{};
	ibi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	UNREF(ici);
	UNREF(ibi);

	VkMemoryAllocateFlagsInfo allocFlagsInfo = makeVkStruct();
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	void_ptr pNext = deviceAddress ? &allocFlagsInfo : nullptr;

	for (uint32_t chunk = 0u; chunk < chunkCount; ++chunk)
	{
		VkDeviceMemory		memory = VK_NULL_HANDLE;
		VkStruct<VkMemoryAllocateInfo>	allocInfo(pNext);

		allocInfo.allocationSize = allocationSize;
		allocInfo.memoryTypeIndex = memoryTypeIndex;

		VKASSERTMSG(vkAllocateMemory(*device, &allocInfo, callbacks, &memory), "failed to allocate buffer memory!");

		ZDistType<SizeSecond, VkDeviceSize> chunkSize = allocationSize;
		if (false == sparse)
			chunkSize = desiredSize;
		else if (chunk + 1u == chunkCount)
			chunkSize = desiredSize % allocationSize;

		allocations.at(chunk) = ZDeviceMemory::create(memory, device, callbacks, properties, allocationSize, chunkSize, nullptr);
	}

	return allocations;
}

add_ptr<uint8_t> mapMemory (ZDeviceMemory memory)
{
	const VkMemoryPropertyFlags	props = memory.getParam<VkMemoryPropertyFlags>();
	ASSERTION(props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();
	uint8_t**		pointer	= &memory.getParamRef<add_ptr<uint8_t>>();
	if (vkMapMemory(*device, *memory, 0, size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(pointer)) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to map memory");
	}

	return memory.getParam<add_ptr<uint8_t>>();
}

void unmapMemory (ZDeviceMemory memory)
{
	ZDevice	device = memory.getParam<ZDevice>();
	vkUnmapMemory(*device, *memory);
	memory.getParamRef<add_ptr<uint8_t>>() = nullptr;
}

void flushMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkStruct<VkMappedMemoryRange>	range;
	range.memory	= *memory;
	range.offset	= 0u;
	range.size	= size;

	vkFlushMappedMemoryRanges(*device, 1u, &range);
}

void invalidateMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkStruct<VkMappedMemoryRange>	range;
	range.memory	= *memory;
	range.offset	= 0u;
	range.size		= size;

	vkInvalidateMappedMemoryRanges(*device, 1u, &range);
}

Alloc::Alloc (add_cref<std::vector<ZDeviceMemory>> allocs)
	: m_allocs		(allocs)
	, m_count		(data_count(allocs))
	, m_total		(0u)
	, m_chunkSize	(m_count ? allocs.at(0).getParam<ZDistType<SizeSecond, VkDeviceSize>>()() : VkDeviceSize(0u))
{
	for (add_cref<ZDeviceMemory> mem : m_allocs)
		m_total += mem.getParam<ZDistType<SizeSecond, VkDeviceSize>>();
}

Alloc::Alloc(ZBuffer buffer)
	: Alloc(buffer.getParamRef<std::vector<ZDeviceMemory>>())
{
}

Alloc::~Alloc ()
{
	for (add_ref<std::pair<const uint32_t, add_ptr<uint8_t>>> mem : m_map)
	{
		unmapMemory(m_allocs.at(mem.first));
	}
	m_map.clear();
}

add_ptr<uint8_t> Alloc::getMemory (uint32_t index)
{
	add_ptr<uint8_t> ptr = m_map[index];
	if (nullptr == ptr)
	{
		ptr = mapMemory(m_allocs.at(index));
		m_map[index] = ptr;
	}
	return ptr;
}

long Alloc::getTotal (std::size_t elementSize) const
{
	return long((m_total / elementSize) * elementSize);
}

Alloc::Impl::Impl (add_ref<Alloc> alloc, long pos, uint32_t size)
	: m_alloc	(alloc)
	, m_pos		(pos)
	, m_size	(size)
{
}

void Alloc::Impl::inc() { m_pos += m_size; }
void Alloc::Impl::dec() { m_pos -= m_size; }
Alloc::Impl::Value Alloc::Impl::val ()
{
#if 1
	const uint32_t pv = uint32_t(make_unsigned(m_pos));
	if (m_pos >= 0 && (pv + m_size <= m_alloc.m_total))
	{
		Value v{};
		const uint32_t chunk = pv / uint32_t(m_alloc.m_chunkSize);
		const uint32_t offset = pv % uint32_t(m_alloc.m_chunkSize);
		v.ptr1 = m_alloc.getMemory(chunk) + offset;
		if (offset + m_size <= m_alloc.m_chunkSize)
			v.len1 = m_size;
		else
		{
			v.len1 = uint32_t(m_alloc.m_chunkSize - offset);
			v.ptr2 = m_alloc.getMemory(chunk + 1u);
			v.len2 = m_size - v.len1;
		}
		return v;
	}
#else
	Value value{};
	VkDeviceSize loAllocSize = 0u;

	for (uint32_t i = 0u; i < m_alloc.m_count; ++i)
	{
		const VkDeviceSize hiAllocSize =
			loAllocSize + m_alloc.m_allocs.at(i).getParam<ZDistType<SizeSecond, VkDeviceSize>>();

		if (m_pos >= 0 && make_unsigned(m_pos) >= loAllocSize && make_unsigned(m_pos) < hiAllocSize)
		{
			const uint32_t offset = uint32_t(make_unsigned(m_pos) - loAllocSize);
			value.ptr1 = m_alloc.getMemory(i) + offset;

			if (make_unsigned(m_pos) + m_size <= hiAllocSize)
			{
				value.len1 = m_size;
				value.ptr2 = nullptr;
				value.len2 = 0u;
				return value;
			}
			else if (i + 1u < m_alloc.m_count)
			{
				value.len1 = uint32_t(hiAllocSize - make_unsigned(m_pos));
				value.ptr2 = m_alloc.getMemory(i + 1u);
				value.len2 = m_size - value.len1;
				return value;
			}
		}

		loAllocSize = hiAllocSize;
	}
#endif
	ASSERTFALSE("Out of range");
	return Value{};
}
void Alloc::Impl::verify (add_cref<Impl> other) const
{
	add_cptr<char> errmsg = "Comparison of different iterators";
	ASSERTMSG(&m_alloc == &other.m_alloc, errmsg);
	ASSERTMSG(m_size == other.m_size, errmsg);
}
long Alloc::Impl::diff (add_cref<Impl> other) const
{
	verify(other);
	return (long(other.m_pos) - long(m_pos)) / m_size;
}
bool Alloc::Impl::eq (add_cref<Impl> other) const
{
	return &m_alloc == &other.m_alloc && m_pos == other.m_pos && m_size == other.m_size;
}
void Alloc::Impl::assign (add_cref<Impl> other)
{
	verify(other);
	m_pos = other.m_pos;
}

} // namespace vtf
