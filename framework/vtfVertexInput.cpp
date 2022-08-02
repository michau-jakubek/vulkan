#include "vtfVertexInput.hpp"
#include "vtfContext.hpp"
#include "vtfCUtils.hpp"
#include <sstream>

namespace vtf
{

void assertVertexBinding (const VulkanContext& ctx, uint32_t binding)
{
	VkPhysicalDeviceProperties p;
	vkGetPhysicalDeviceProperties(*ctx.physicalDevice, &p);
	ASSERTMSG(binding < p.limits.maxVertexInputBindings, "Binding exceeds maxVertexInput limit");
}

ZDeviceMemory makeEmptyMemory (const VulkanContext& ctx)
{
	return ZDeviceMemory::createEmpty(ctx.device, ctx.callbacks, {}, 0, nullptr);
}
ZBuffer makeEmptyBuffer (const VulkanContext& ctx)
{
	return ZBuffer::createEmpty(ctx.device, ctx.callbacks, {}, makeEmptyMemory(ctx), 0);
}

VertexBinding::VertexBinding (const VertexInput& vertexInput)
	: vertexInput		(vertexInput)
	, bufferType		(m_bufferType)
	, m_bufferType		(BufferType::Undefined)
	, m_buffer			(makeEmptyBuffer(vertexInput.context))
	, m_descriptions	()
{
}

VertexBinding::VertexBinding (VertexBinding&& other)
	: vertexInput		(other.vertexInput)
	, bufferType		(m_bufferType)
	, m_bufferType		(other.m_bufferType)
	, m_buffer			(other.m_buffer)
	, m_descriptions	(std::move(other.m_descriptions))
{
}

ZBuffer VertexBinding::getBuffer () const
{
	ASSERTION(BufferType::Internal == m_bufferType);
	return m_buffer;
}

VertexBinding::Location VertexBinding::declareAttributes_ (const AttrFwd* fwd, const uint32_t count)
{
	if (BufferType::Undefined == m_bufferType)
		m_bufferType = BufferType::External;
	else if (BufferType::External != m_bufferType)
	{
		std::ostringstream s;
		s << "Cannot declare vertex attribs in binding (" << binding << ") with buffer";
		s.flush();
		ASSERTMSG(VK_FALSE, s.str());
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

VertexBinding::Location VertexBinding::addAttributes_ (const AttrFwd* fwd, const uint32_t count)
{
	if (BufferType::Undefined == m_bufferType)
		m_bufferType = BufferType::Internal;
	else if (BufferType::Internal != m_bufferType)
	{
		std::ostringstream s;
		s << "Cannot declare vertex attribs in binding (" << binding << ") with no buffer";
		s.flush();
		ASSERTMSG(VK_FALSE, s.str());
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
			ASSERTMSG(VK_FALSE, "Element count of all attribs must be equal");
		}
	}

	const ZBufferUsageFlags		usage		= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const ZMemoryPropertyFlags	props		(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	ZDevice						device		= vertexInput.context.device;
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

	ZDeviceMemory	newMemory	= newBuffer.getParam<ZDeviceMemory>();
	VKASSERT(vkMapMemory(*device, *newMemory, 0, newBufferSize, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)), "");

	if (oldStride)
	{
		const VkDeviceSize	oldBufferSize	= elementCount * oldStride;
		ZDeviceMemory oldMemory = m_buffer.getParam<ZDeviceMemory>();
		VKASSERT(vkMapMemory(*device, *oldMemory, 0, oldBufferSize, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&bindingSource)), "");

		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *oldMemory;
		range.offset	= 0;
		range.size		= oldBufferSize;
		VKASSERT2(vkInvalidateMappedMemoryRanges(*device, 1, &range));

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

	VkMappedMemoryRange	range{};
	range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.memory	= *newMemory;
	range.offset	= 0;
	range.size		= newBufferSize;
	VKASSERT2(vkFlushMappedMemoryRanges(*device, 1, &range));
	vkUnmapMemory(*device, *newMemory);

	if (oldStride)
	{
		ZDeviceMemory oldMemory = m_buffer.getParam<ZDeviceMemory>();
		vkUnmapMemory(*device, *oldMemory);
	}

	this->stride = newStride;
	m_buffer = newBuffer;

	return location;
}

VertexBinding& VertexInput::binding (uint32_t binding)
{
	assertVertexBinding(context, binding);
	auto b = std::find_if(m_freeBindings.begin(), m_freeBindings.end(),
						  [&](const VertexBinding& b) { return b.binding == binding; });
	if (m_freeBindings.end() == b)
	{
		m_freeBindings.emplace_back(*this);
		b = iterator_from_index(m_freeBindings, binding);
		b->binding		= binding;
		b->stride		= 0u;
		b->inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	}
	return *b;
}

VkPipelineVertexInputStateCreateInfo VertexInput::createPipelineVertexInputStateCreateInfo () const
{
	ASSERTMSG(m_freeBindings.size(), "There is no vertex inputs");

	uint32_t bindingCount		= 0;
	uint32_t descriptionCount	= 0;
	for (const VertexBinding& bind : m_freeBindings)
	{
		ASSERTMSG(bind.bufferType != VertexBinding::BufferType::Undefined,
				  "Unable to process empty binding");
		bindingCount = std::max(bind.binding, bindingCount);
		descriptionCount += (uint32_t)bind.m_descriptions.size();
	}
	bindingCount += 1;

	m_pipeBindings.resize(bindingCount);
	for (uint32_t i = 0; i < bindingCount; ++i) m_pipeBindings[i] = {};

	m_pipeSescriptions.reserve(descriptionCount);
	for (const VertexBinding& bind : m_freeBindings)
	{
		m_pipeBindings[bind.binding] = bind;
		m_pipeSescriptions.insert(m_pipeSescriptions.end(), bind.m_descriptions.begin(), bind.m_descriptions.end());
	}

	VkPipelineVertexInputStateCreateInfo	pisc{};
	pisc.sType								= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pisc.pNext								= nullptr;
	pisc.flags								= 0;
	pisc.vertexBindingDescriptionCount		= bindingCount;
	pisc.pVertexBindingDescriptions			= m_pipeBindings.data();
	pisc.vertexAttributeDescriptionCount	= descriptionCount;
	pisc.pVertexAttributeDescriptions		= m_pipeSescriptions.data();

	return pisc;
}

uint32_t VertexInput::getAttributeCount (uint32_t binding) const
{
	for (const VertexBinding& bind : m_freeBindings)
	{
		if (bind.binding == binding && bind.m_descriptions.size())
		{
			const uint32_t count = bind.m_descriptions[0].count;
			return count;
		}
	}
	return INVALID_UINT32;
}

std::vector<VkBuffer> VertexInput::getVertexBuffers (std::initializer_list<ZBuffer> externalBuffers) const
{
	ASSERTMSG(m_freeBindings.size(), "There is no vertex inputs");

	uint32_t bindingCount			= 0;
	uint32_t internalBindingCount	= 0;
	for (const VertexBinding& bind : m_freeBindings)
	{
		ASSERTMSG(bind.bufferType != VertexBinding::BufferType::Undefined,
				  "Unable to process empty binding");

		if (bind.bufferType == VertexBinding::BufferType::Internal)
			internalBindingCount = internalBindingCount + 1;

		bindingCount = std::max(bind.binding, bindingCount);
	}
	bindingCount = bindingCount + 1;

	const uint32_t externalBindingCount = bindingCount - internalBindingCount;
	ASSERTMSG(externalBindingCount <= static_cast<uint32_t>(externalBuffers.size()),
			  "External buffers list must be greater or equal to free binding count");

	std::vector<VkBuffer> buffers(bindingCount);
	auto itExternalBuffers = externalBuffers.begin();

	for (uint32_t i = 0; i < bindingCount; ++i)
	{
		auto bind = std::find_if(m_freeBindings.begin(), m_freeBindings.end(),
								 [i](const VertexBinding& b) {
									return b.binding == i;	});
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

void VertexInput::clean ()
{
	m_freeBindings.clear();
	m_pipeBindings.clear();
	m_pipeSescriptions.clear();
}

} // namespace vtf
