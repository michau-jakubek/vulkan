#include "vtfVertexInput.hpp"
#include "vtfZBuffer.hpp"
#include "vtfContext.hpp"
#include "vtfCUtils.hpp"
#include "vtfStructUtils.hpp"
#include <sstream>

namespace vtf
{

void assertVertexBinding (add_cref<ZDevice> device, uint32_t binding)
{
	VkPhysicalDeviceProperties p;
	vkGetPhysicalDeviceProperties(*device.getParam<ZPhysicalDevice>(), &p);
	ASSERTMSG(binding < p.limits.maxVertexInputBindings, "Binding exceeds maxVertexInput limit");
}

VertexBinding::VertexBinding (add_cref<VertexInput> vertexInput)
	: vertexInput		(vertexInput)
	, bufferType		(m_bufferType)
	, m_bufferType		(BufferType::Undefined)
	, m_buffer			()
	, m_descriptions	()
{
	binding		= INVALID_UINT32;
	stride		= INVALID_UINT32;
	inputRate	= VK_VERTEX_INPUT_RATE_MAX_ENUM;
}

VertexBinding::VertexBinding (VertexBinding&& other) noexcept
	: VkVertexInputBindingDescription	(other)
	, vertexInput						(other.vertexInput)
	, bufferType						(m_bufferType)
	, m_bufferType						(other.m_bufferType)
	, m_buffer							(other.m_buffer)
	, m_descriptions					(std::move(other.m_descriptions))
{
}

ZBuffer VertexBinding::getBuffer () const
{
	ASSERTION(BufferType::Internal == m_bufferType);
	return m_buffer;
}

VkVertexInputBindingDescription2EXT VertexBinding::asVkVertexInputBindingDescription2EXT () const
{
	VkVertexInputBindingDescription2EXT ext = makeVkStruct();
	ext.binding		= binding;
	ext.stride		= stride;
	ext.inputRate	= inputRate;
	ext.divisor		= 1u;
	return ext;
}

VertexBinding::Location VertexBinding::declareAttributes_ (const AttrFwd* fwd, const uint32_t count)
{
	if (BufferType::Undefined == m_bufferType)
		m_bufferType = BufferType::External;
	else if (BufferType::External != m_bufferType)
	{
		ASSERTFALSE("Cannot declare vertex attribs in binding (", binding, ") with buffer");
	}

	const Location	location	= static_cast<Location>(m_descriptions.size());
	uint32_t		offset		= m_descriptions.empty() ? 0 : (m_descriptions.back().offset + m_descriptions.back().sizeOf);

	// Update stride
	{
		uint32_t fwdStride = 0;
		for (uint32_t i = 0; i < count; ++i)
			fwdStride += fwd[i].sizeOf;
		this->stride += fwdStride;
	}

	m_descriptions.resize(location + count);
	for (uint32_t i = 0; i < count; ++i)
	{
		add_ref<Description> desc = m_descriptions[location + i];
		desc.location	= location + i;
		desc.binding	= this->binding;
		desc.format		= fwd[i].format;
		desc.offset		= offset;
		desc.sizeOf		= fwd[i].sizeOf;
		desc.count		= 0;

		offset += fwd[i].sizeOf;
	}

	return location;
}

VertexBinding::Location VertexBinding::appendAttribute (uint32_t location, VkFormat format, uint32_t offset)
{
	if (BufferType::Undefined == m_bufferType)
		m_bufferType = BufferType::External;
	else if (BufferType::External != m_bufferType)
	{
		ASSERTFALSE("Cannot declare vertex attribs in binding (", binding, ") with buffer");
	}

	Description d;
	d.location	= location;
	d.binding	= this->binding;
	d.format	= format;
	d.offset	= offset;
	d.sizeOf	= 1;
	d.count		= 0;

	m_descriptions.push_back(d);

	return (uint32_t)m_descriptions.size();
}

VertexBinding::Location VertexBinding::addAttributes_ (const AttrFwd* fwd, const uint32_t count)
{
	if (BufferType::Undefined == m_bufferType)
		m_bufferType = BufferType::Internal;
	else if (BufferType::Internal != m_bufferType)
	{
		ASSERTFALSE("Cannot declare vertex attribs in binding (", binding, ") with no buffer");
	}

	const uint32_t elementCount = fwd[0].count;

	// Verify element count
	{
		bool differs = false;
		for (uint32_t i = 0; i < count; ++i)
		{
			if (fwd[i].count != elementCount)
			{
				differs = true;
				break;
			}
		}
		if (m_descriptions.size())
		{
			differs |= (elementCount != m_descriptions.back().count);
		}
		if (differs)
		{
			ASSERTFALSE("Element count of all attribs must be equal");
		}
	}

	const ZBufferUsageFlags		usage		= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const ZMemoryPropertyFlags	props		(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	ZDevice						device		= vertexInput.device;
	const Location				location	= static_cast<Location>(m_descriptions.size());
	uint32_t					offset		= m_descriptions.empty() ? 0 : (m_descriptions.back().offset + m_descriptions.back().sizeOf);

	m_descriptions.resize(location + count);
	for (uint32_t i = 0; i < count; ++i)
	{
		add_ref<Description> desc = m_descriptions[location + i];
		desc.location	= location + i;
		desc.binding	= this->binding;
		desc.format		= fwd[i].format;
		desc.offset		= offset;
		desc.sizeOf		= fwd[i].sizeOf;
		desc.count		= fwd[i].count;

		offset += fwd[i].sizeOf;
	}

	uint32_t attributesStride	= 0;
	{
		for (uint32_t i = 0; i < count; ++i)
			attributesStride += fwd[i].sizeOf;
	}
	const uint32_t		oldStride		= this->stride;
	const uint32_t		newStride		= oldStride + attributesStride;
	const VkDeviceSize	newBufferSize	= elementCount * newStride;
	ZBuffer				newBuffer		= createBuffer(device, newBufferSize, usage, props);

	uint8_t*			dst				= nullptr;
	uint8_t*			bindingSource	= nullptr;
	const uint8_t*		inputSource		= nullptr;

	ZDeviceMemory	newMemory	= bufferGetMemory(newBuffer, 0u);
	VKASSERT(vkMapMemory(*device, *newMemory, 0, newBufferSize, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)));

	if (oldStride)
	{
		const VkDeviceSize	oldBufferSize	= elementCount * oldStride;
		ZDeviceMemory oldMemory = bufferGetMemory(m_buffer, 0u);
		VKASSERT(vkMapMemory(*device, *oldMemory, 0, oldBufferSize, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&bindingSource)));

		/*
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *oldMemory;
		range.offset	= 0;
		range.size		= oldBufferSize;
		VKASSERT2(vkInvalidateMappedMemoryRanges(*device, 1, &range));
		*/

		for (uint32_t elem = 0; elem < elementCount; ++elem)
		{
			std::memcpy(&dst[ elem * newStride ], &bindingSource[ elem * oldStride ], oldStride);
		}
	}

	/*
	 *	b1,b2,b3
	 *	p1,p2,p3	b1,p1,c1, b2,p2,c2, b3,p3,c3
	 *	c1,c2,c3
	 */

	uint32_t attributeOffset = oldStride;
	for (uint32_t attribute = 0; attribute < count; ++attribute)
	{
		inputSource = static_cast<const uint8_t*>(fwd[attribute].ptr);
		uint32_t		srcOffset	= 0;
		uint32_t		dstOffset			= attributeOffset;
		const uint32_t	attributeSize		= fwd[attribute].sizeOf;
		for (uint32_t elem = 0; elem < elementCount; ++elem)
		{
			std::memcpy(&dst[dstOffset], &inputSource[srcOffset], attributeSize);
			srcOffset += attributeSize;
			dstOffset += newStride;
		}
		attributeOffset += attributeSize;
	}

	/*
	VkMappedMemoryRange	range{};
	range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.memory	= *newMemory;
	range.offset	= 0;
	range.size		= newBufferSize;
	VKASSERT2(vkFlushMappedMemoryRanges(*device, 1, &range));
	*/
	vkUnmapMemory(*device, *newMemory);

	if (oldStride)
	{
		ZDeviceMemory oldMemory = bufferGetMemory(m_buffer, 0u);
		vkUnmapMemory(*device, *oldMemory);
	}

	this->stride = newStride;
	m_buffer = newBuffer;

	return location;
}

VertexBinding& VertexInput::binding (uint32_t binding, uint32_t stride, VkVertexInputRate rate)
{
	assertVertexBinding(device, binding);
	auto b = std::find_if(m_freeBindings.begin(), m_freeBindings.end(),
						  [&](const VertexBinding& b) { return b.binding == binding; });
	if (m_freeBindings.end() == b)
	{
		m_freeBindings.emplace_back(*this);
		const uint32_t	last = (uint32_t)(m_freeBindings.size() - 1u);
		b = iterator_from_index(m_freeBindings, last);
		b->binding		= binding;
		b->stride		= stride;
		b->inputRate	= rate;
	}
	return *b;
}

VkVertexInputAttributeDescription VertexBinding::Description::asVkVertexInputAttributeDescription () const
{
	add_cref<VkVertexInputAttributeDescription> result(*this);
	return result;
}

VkVertexInputAttributeDescription2EXT VertexBinding::Description::asVkVertexInputAttributeDescription2EXT () const
{
	VkVertexInputAttributeDescription2EXT ext = makeVkStruct();
	ext.location	= location;
	ext.binding		= binding;
	ext.format		= format;
	ext.offset		= offset;
	return ext;
}

ZPipelineVertexInputStateCreateInfo::ZPipelineVertexInputStateCreateInfo ()
	: m_pipeBindings(), m_pipeDescriptions()
{
}

void ZPipelineVertexInputStateCreateInfo::swap (ZPipelineVertexInputStateCreateInfo&& other)
{
	m_pipeBindings.swap(other.m_pipeBindings);
	m_pipeDescriptions.swap(other.m_pipeDescriptions);
}

ZPipelineVertexInputStateCreateInfo::ZPipelineVertexInputStateCreateInfo (add_cref<VertexBinding> vertexBinding)
{
	m_pipeBindings.resize(1u, vertexBinding);
	m_pipeDescriptions.insert(m_pipeDescriptions.end(),
							  vertexBinding.m_descriptions.begin(), vertexBinding.m_descriptions.end());
}

ZPipelineVertexInputStateCreateInfo::ZPipelineVertexInputStateCreateInfo (add_cref<VertexInput> vertexInput)
	: m_pipeBindings(), m_pipeDescriptions()
{
	add_cref<decltype(vertexInput.m_freeBindings)> freeBindings = vertexInput.m_freeBindings;

	ASSERTMSG(freeBindings.size(), "There is no vertex inputs");

	uint32_t bindingCount		= 0;
	uint32_t descriptionCount	= 0;
	for (const VertexBinding& bind : freeBindings)
	{
		ASSERTMSG(bind.bufferType != VertexBinding::BufferType::Undefined,
				  "Unable to process empty binding");
		bindingCount = std::max(bind.binding, bindingCount);
		descriptionCount += (uint32_t)bind.m_descriptions.size();
	}
	bindingCount += 1;

	m_pipeBindings.resize(bindingCount);
	m_pipeDescriptions.reserve(descriptionCount);
	for (const VertexBinding& bind : freeBindings)
	{
		m_pipeBindings[bind.binding] = bind;
		m_pipeDescriptions.insert(m_pipeDescriptions.end(), bind.m_descriptions.begin(), bind.m_descriptions.end());
	}
}

VkPipelineVertexInputStateCreateInfo ZPipelineVertexInputStateCreateInfo::operator ()() const
{
	VkPipelineVertexInputStateCreateInfo	pisc = makeVkStruct();
	pisc.flags								= VkPipelineVertexInputStateCreateFlags(0);
	pisc.vertexBindingDescriptionCount		= data_count(m_pipeBindings);
	pisc.pVertexBindingDescriptions			= data_or_null(m_pipeBindings);
	pisc.vertexAttributeDescriptionCount	= data_count(m_pipeDescriptions);
	pisc.pVertexAttributeDescriptions		= data_or_null(m_pipeDescriptions);
	return pisc;
}

ZVertexInput2EXT::ZVertexInput2EXT (add_cref<VertexInput> vertexInput)
	: bindingDescriptions	(vertexInput.getBindingCount())
	, attributeDescriptions	(vertexInput.getAttributeCount())
{
	for (uint32_t b = 0u, c = 0u; b < bindingDescriptions.size(); ++b)
	{
		add_cref<VertexBinding> vb = vertexInput.m_freeBindings.at(b);
		bindingDescriptions.at(b) = vb.asVkVertexInputBindingDescription2EXT();
		for (add_cref<VertexBinding::Description> a : vb.m_descriptions)
		{
			attributeDescriptions.at(c++) = a.asVkVertexInputAttributeDescription2EXT();
		}
	}
}

uint32_t VertexInput::getBindingCount () const
{
	return static_cast<uint32_t>(m_freeBindings.size());
}

uint32_t VertexInput::getVertexCount (uint32_t binding) const
{
	for (add_cref<VertexBinding> bind : m_freeBindings)
	{
		if (bind.binding == binding && bind.m_descriptions.size())
		{
			const uint32_t count = bind.m_descriptions[0].count;
			return count;
		}
	}
	return INVALID_UINT32;
}

uint32_t VertexInput::getAttributeCount (uint32_t binding) const
{
	for (add_cref<VertexBinding> bind : m_freeBindings)
	{
		if (bind.binding == binding && bind.m_descriptions.size())
		{
			const uint32_t count = data_count(bind.m_descriptions);
			return count;
		}
	}
	return INVALID_UINT32;
}

uint32_t VertexInput::getAttributeCount () const
{
	uint32_t count = 0;
	for (const VertexBinding& bind : m_freeBindings)
		count += data_count(bind.m_descriptions);
	return count;
}

std::vector<VkBuffer> VertexInput::getVertexBuffers (std::initializer_list<ZBuffer> externalBuffers) const
{
	ASSERTMSG(m_freeBindings.size(), "There is no vertex inputs");

	uint32_t maxBinding				= 0;
	uint32_t internalBindingCount	= 0;
	uint32_t externalBindingCount	= 0; UNREF(externalBindingCount);
	for (const VertexBinding& bind : m_freeBindings)
	{
		switch (bind.bufferType)
		{
		case VertexBinding::BufferType::Internal: ++internalBindingCount; break;
		case VertexBinding::BufferType::External: ++externalBindingCount; break;
		case VertexBinding::BufferType::Undefined: ASSERTFALSE("Unable to process empty binding");
		}

		maxBinding = std::max(bind.binding, maxBinding);
	}

	const auto externalBufferCount = externalBuffers.size();
	ASSERTMSG(externalBufferCount >= (maxBinding + 1u - internalBindingCount),
			  "externalBuffers.size() must be greater or equal unbound bindings count");

	std::vector<VkBuffer> buffers(maxBinding + 1u, VK_NULL_HANDLE);
	auto itExternalBuffers = externalBuffers.begin();

	for (uint32_t i = 0; i <= maxBinding; ++i)
	{
		auto bind = std::find_if(m_freeBindings.begin(), m_freeBindings.end(),
								 [i](const VertexBinding& b)
								 {
									return b.binding == i;
								 });
		const bool isInternal = (m_freeBindings.end() != bind && bind->bufferType == VertexBinding::BufferType::Internal);
		buffers[i] = isInternal ? *bind->m_buffer : **itExternalBuffers++;
	}

	return buffers;
}

std::vector<VkDeviceSize> VertexInput::getVertexOffsets	() const
{
	uint32_t bindingCount			= 0;
	for (const VertexBinding& bind : m_freeBindings)
		bindingCount = std::max(bind.binding, bindingCount);

	return std::vector<VkDeviceSize>(( bindingCount + 1 ), 0u);
}

void VertexInput::clear ()
{
	m_freeBindings.clear();
}

template<> ZBuffer createIndexBuffer<uint32_t> (ZDevice device, uint32_t indexCount)
{
	return createIndexBuffer(device, indexCount, VK_INDEX_TYPE_UINT32);
}

template<> ZBuffer createIndexBuffer<uint16_t> (ZDevice device, uint32_t indexCount)
{
	return createIndexBuffer(device, indexCount, VK_INDEX_TYPE_UINT16);
}

template<class IndexType> // Actually uint32_t or uint16_t only
ZBuffer createIndexBuffer (ZDevice device, const std::vector<IndexType>& indices, uint32_t repeatCount)
{
	ASSERTMSG(repeatCount, "RepeatCount must be positive number");
	ZBuffer buffer = createIndexBuffer(device, (data_count(indices) * repeatCount), index_type_to_vk_index_type<IndexType>);
	BufferTexelAccess<IndexType>	access(buffer, data_count(indices), repeatCount);
	for (uint32_t r = 0u; r < repeatCount; ++r)
	for (uint32_t i = 0u; i < data_count(indices); ++i)
	{
		access.at(i, r) = indices[i];
	}
	return buffer;
}
template ZBuffer createIndexBuffer<uint32_t> (ZDevice, const std::vector<uint32_t>&, uint32_t);
template ZBuffer createIndexBuffer<uint16_t> (ZDevice, const std::vector<uint16_t>&, uint32_t);

} // namespace vtf
