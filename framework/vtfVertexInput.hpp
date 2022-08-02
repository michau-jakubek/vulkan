#ifndef __VTF_VERTEX_INPUT_HPP_INCLUDED__
#define __VTF_VERTEX_INPUT_HPP_INCLUDED__

#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfZDeletable.hpp"
#include "vtfZBuffer.hpp"
#include "vtfVector.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace vtf
{
class VulkanContext;
class VertexInput;

struct VertexBinding : public VkVertexInputBindingDescription
{
	typedef uint32_t Location;

	enum class BufferType
	{
		Undefined,
		Internal,
		External
	};

	VertexBinding (const VertexInput& vertexInput);
	VertexBinding (const VertexBinding&) = delete;
	VertexBinding (VertexBinding&& other);

	const VertexInput&	vertexInput;
	const BufferType&	bufferType;

	template<class Attr, class... OtherAttrs>
		Location	declareAttributes	();
	template<class Attr, class... OtherAttrs>
		uint32_t	addAttributes		(const std::vector<Attr>& attr, const std::vector<OtherAttrs>&... others);

	ZBuffer			getBuffer			() const;

	struct AttrFwd
	{
		uint32_t	sizeOf;
		uint32_t	count;
		VkFormat	format;
		const void*	ptr;
	};

protected:
	Location		declareAttributes_	(const AttrFwd* fwd, const uint32_t count);
	Location		addAttributes_		(const AttrFwd* fwd, const uint32_t count);

private:
	friend class VertexInput;

	struct Description : public VkVertexInputAttributeDescription
	{
		uint32_t	sizeOf;
		uint32_t	count;
	};

	BufferType					m_bufferType;
	ZBuffer						m_buffer;
	std::vector<Description>	m_descriptions;
};

typedef VertexBinding::AttrFwd AttrFwd;

class VertexInput
{
public:
	VertexInput (const VulkanContext& ctx)
		: context(ctx) {}
	VertexInput (const VertexInput&) = delete;
	VertexInput (VertexInput&&) = delete;

	const VulkanContext&		context;

	VertexBinding&				binding				(uint32_t binding);
	std::vector<VkBuffer>		getVertexBuffers	(std::initializer_list<ZBuffer> externalBuffers = {}) const;
	std::vector<VkDeviceSize>	getVertexOffsets	() const;
	uint32_t					getAttributeCount	(uint32_t binding) const;
	void						clean				();

	VkPipelineVertexInputStateCreateInfo createPipelineVertexInputStateCreateInfo() const;

private:
	std::vector<VertexBinding>								m_freeBindings;
	mutable std::vector<VkVertexInputBindingDescription>	m_pipeBindings;
	mutable std::vector<VkVertexInputAttributeDescription>	m_pipeSescriptions;
};

template<class Attr>
void populateForwardAttribute (AttrFwd* fwd, uint32_t at,
							   const std::vector<Attr>& attr)
{
	if (at) ASSERTION(attr.size() == fwd[at-1].count);
	fwd[at].sizeOf	= sizeof(Attr);
	fwd[at].count	= (uint32_t)attr.size();
	fwd[at].format	= type_to_vk_format<Attr>;
	fwd[at].ptr		= attr.data();
}
template<class Attr, class AttrNext, class... OtherAttrs>
void populateForwardAttribute (AttrFwd* fwd, uint32_t at,
							   const std::vector<Attr>& attr,
							   const std::vector<AttrNext>& next,
							   const std::vector<OtherAttrs>&... others)
{
	populateForwardAttribute(fwd, at, attr);
	populateForwardAttribute(fwd, at+1, next, others...);
}
template<class Attr, class... OtherAttrs>
uint32_t VertexBinding::addAttributes (const std::vector<Attr>& attr, const std::vector<OtherAttrs>&... others)
{
	AttrFwd	fwd[1 + (sizeof...(OtherAttrs))];
	populateForwardAttribute(fwd, 0, attr, others...);
	return addAttributes_(fwd, 1 + (sizeof...(OtherAttrs)));
}

template<class Attr>
void populateForwardAttribute (AttrFwd* fwd, uint32_t at)
{
	fwd[at].sizeOf	= sizeof(Attr);
	fwd[at].count	= 0;
	fwd[at].format	= type_to_vk_format<Attr>;
	fwd[at].ptr		= nullptr;
}
template<class Attr, class AttrNext, class... OtherAttrs>
void populateForwardAttribute (AttrFwd* fwd, uint32_t at)
{
	populateForwardAttribute<Attr>(fwd, at);
	populateForwardAttribute<AttrNext, OtherAttrs...>(fwd, at+1);
}
template<class Attr, class... OtherAttrs>
uint32_t VertexBinding::declareAttributes	()
{
	AttrFwd	fwd[1 + (sizeof...(OtherAttrs))];
	populateForwardAttribute<Attr, OtherAttrs...>(fwd, 0);
	return declareAttributes_(fwd, 1 + (sizeof...(OtherAttrs)));
}

} // namespace vtf

#endif // __VTF_VERTEX_INPUT_HPP_INCLUDED__
