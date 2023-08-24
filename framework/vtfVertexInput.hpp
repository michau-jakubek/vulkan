#ifndef __VTF_VERTEX_INPUT_HPP_INCLUDED__
#define __VTF_VERTEX_INPUT_HPP_INCLUDED__

#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfZDeletable.hpp"
#include "vtfZBuffer.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfVector.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace vtf
{
class VulkanContext;
struct VertexBinding;
class VertexInput;

struct ZPipelineVertexInputStateCreateInfo : VkPipelineVertexInputStateCreateInfo
{
	ZPipelineVertexInputStateCreateInfo ();
	ZPipelineVertexInputStateCreateInfo (add_cref<VertexBinding> vertexBinding);
	ZPipelineVertexInputStateCreateInfo (add_cref<VertexInput> vertexInput);
	VkPipelineVertexInputStateCreateInfo operator ()() const;
	virtual ~ZPipelineVertexInputStateCreateInfo () = default;
	void swap(ZPipelineVertexInputStateCreateInfo&& other);
protected:
	std::vector<VkVertexInputBindingDescription>	m_pipeBindings;
	std::vector<VkVertexInputAttributeDescription>	m_pipeDescriptions;
};

struct VertexBinding : public VkVertexInputBindingDescription
{
	typedef uint32_t Location;

	enum class BufferType
	{
		Undefined,
		Internal,
		External
	};

	VertexBinding (add_cref<VertexInput> vertexInput);
	VertexBinding (add_cref<VertexBinding>) = delete;
	VertexBinding (VertexBinding&& other);
	friend struct ZPipelineVertexInputStateCreateInfo;

	add_cref<VertexInput>	vertexInput;
	add_cref<BufferType>	bufferType;

	/**
	 * NOTE that the location numbers start from 0 and continue numbering within single binding.
	 * There must not be two different bindings with the same location(s).
	 */
	template<class Attr, class... OtherAttrs>
		Location	declareAttributes	();
	Location		appendAttribute		(uint32_t location, VkFormat format, uint32_t offset);
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
	VertexInput (add_cref<ZDevice> dev)
		: device(dev) {}
	VertexInput (add_cref<VertexInput>) = delete;
	VertexInput (VertexInput&&) = delete;
	friend struct ZPipelineVertexInputStateCreateInfo;

	add_cref<ZDevice>			device;

	add_ref<VertexBinding>		binding				(uint32_t binding, uint32_t stride = 0,
													VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX);
	std::vector<VkBuffer>		getVertexBuffers	(std::initializer_list<ZBuffer> externalBuffers = {}) const;
	std::vector<VkDeviceSize>	getVertexOffsets	() const;
	uint32_t					getBindingCount		() const;
	uint32_t					getAttributeCount	(uint32_t binding) const;
	void						clear				();

protected:
	std::vector<VertexBinding>	m_freeBindings;
};

template<class> std::bad_typeid index_type_to_vk_index_type;
template<> inline constexpr VkIndexType index_type_to_vk_index_type<uint32_t> = VK_INDEX_TYPE_UINT32;
template<> inline constexpr VkIndexType index_type_to_vk_index_type<uint16_t> = VK_INDEX_TYPE_UINT16;

template<class IndexType> // Actually uint32_t or uint16_t only
ZBuffer createIndexBuffer (ZDevice device, uint32_t indexCount);

template<class IndexType> // Actually uint32_t or uint16_t only
ZBuffer createIndexBuffer (ZDevice device, const std::vector<IndexType>& indices, uint32_t repeatCount = 1u);

template<class Attr>
void populateForwardAttribute (AttrFwd* fwd, uint32_t at,
							   const std::vector<Attr>& attr)
{
	if (at) ASSERTION(attr.size() == fwd[at-1].count);
	fwd[at].sizeOf	= sizeof(Attr);
	fwd[at].count	= (uint32_t)attr.size();
	fwd[at].format	= type_to_vk_format<Fmt_<Attr>>;
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
