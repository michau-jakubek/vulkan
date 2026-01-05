#ifndef __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
#define __VTF_ZBUFFER_UTILS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZBarriers.hpp"
#include "vtfZBarriers2.hpp"

#include <algorithm>

namespace vtf
{

/**
 * @brief Create a underlying VkBuffer object and bind it to memory
 * @param device		is the logical device that creates the buffer object
 * @param usage			is a bitmask of VkBufferUsageFlagBits specifying allowed usages of the buffer
 * @param size			is the size in bytes of the buffer to be created
 * @param properties	is a bitmask of VkMemoryPropertyFlagBits specifying memory of the buffer
 * @param flags			is a bitmask of VkBufferCreateFlagBits specifying a creation of the buffer
 * @return				ZBuffer instance with Vulkan handle
 */
ZBuffer			createBuffer	(ZDevice device, VkDeviceSize size, ZBufferUsageFlags usage,
								ZMemoryPropertyFlags properties = ZMemoryPropertyHostFlags,
								ZBufferCreateFlags flags = ZBufferCreateFlags());

template<class X>
ZBuffer			createBuffer	(ZDevice device, uint32_t elements = 1u,
								 ZBufferUsageFlags usage = ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
								 ZMemoryPropertyFlags properties = ZMemoryPropertyHostFlags,
								 ZBufferCreateFlags flags = ZBufferCreateFlags())
{
	extern ZBuffer createTypedBuffer (ZDevice, type_index_with_default, VkDeviceSize,
									  ZBufferUsageFlags, ZMemoryPropertyFlags, ZBufferCreateFlags);

	return createTypedBuffer(device, type_index_with_default::make<X>(), elements * sizeof(X), usage, properties, flags);
}

/**
 * @brief Create a underlying VkBuffer object and bind it to memory
 * @param device		is the logical device that creates the buffer object
 * @param format		describes the format of the hypothetical texel blocks that will be contained in the buffer
 * @param elements		is a number of the hypothetical texel blocks that will be contained in the buffer
 * @param usage			is a bitmask of VkBufferUsageFlagBits specifying allowed usages of the buffer
 * @param properties	is a bitmask of VkMemoryPropertyFlagBits specifying memory of the buffer
 * @param flags			is a bitmask of VkBufferCreateFlagBits specifying a creation of the buffer
 * @return
 */
ZBuffer			createBuffer	(ZDevice device, VkFormat format, uint32_t elements,
								ZBufferUsageFlags usage, ZMemoryPropertyFlags properties, ZBufferCreateFlags flags);

VkDeviceSize	computeBufferSize (VkFormat format, uint32_t imageWidth, uint32_t imageHeight,
								   uint32_t baseMipLevel = 0u, uint32_t mipLevelCount = 1u, uint32_t layerCount = 1u);
/**
 * @brief Create a underlying VkBuffer object and bind it to memory
 * @param image			describes format, size, layers, mip levels, etc. on which the buffer will be created
 * @param usage			is a bitmask of VkBufferUsageFlagBits specifying allowed usages of the buffer
 * @param properties	is a bitmask of VkMemoryPropertyFlagBits specifying memory of the buffer
 * @param flags			is a bitmask of VkBufferCreateFlagBits specifying a creation of the buffer
 * @return
 */
ZBuffer			createBuffer	(ZImage image,
								ZBufferUsageFlags usage = ZBufferUsageStorageFlags,
								ZMemoryPropertyFlags properties = ZMemoryPropertyHostFlags,
								ZBufferCreateFlags flags = ZBufferCreateFlags());

/**
 * @brief   Create index buffer
 * @param   device		is the logical device that creates the buffer object
 * @param   indexCount	defines how many indices the buffer will have
 * @param   indexType	defines the type of indices element
 * @details Specialized implementation you can find in vtfVertexInput.cpp
 * @return
 */
ZBuffer			createIndexBuffer (ZDevice device, uint32_t indexCount, VkIndexType indexType);

ZBuffer			createBufferAndLoadFromImageFile (ZDevice device, add_cref<std::string> imageFileName,
												  ZBufferUsageFlags usage = {}, int desiredChannelCount = 0);
ZBuffer			bufferDuplicate (ZBuffer buffer);
void			bufferFlush		(ZBuffer buffer);

VkDeviceSize	bufferReadData	(ZBuffer buffer, uint8_t* dst, const VkBufferCopy& copy);
VkDeviceSize	bufferReadData	(ZBuffer buffer, uint8_t* dst, VkDeviceSize size = VK_WHOLE_SIZE);

VkDeviceSize	bufferGetSize		(ZBuffer buffer);
VkDeviceAddress	bufferGetAddress	(ZBuffer buffer, uint32_t hintAssertBinding = INVALID_UINT32,
									 VkDescriptorType hintAssertDescriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM);
ZDeviceMemory	bufferGetMemory		(ZBuffer buffer, uint32_t index);
VkDeviceSize	bufferGetMemorySize	(ZBuffer buffer, uint32_t index = 0u);
void			bufferBindMemory	(ZBuffer buffer, ZQueue sparseQueue);

template<class X, class Cast = uint32_t> Cast bufferGetElementCount (ZBuffer buffer)
{
	return static_cast<Cast>(bufferGetSize(buffer) / sizeof(X));
}

void			bufferWriteData (ZBuffer buffer, add_cptr<uint8_t> src, add_cref<VkBufferCopy> copy, bool flush = true);
VkDeviceSize	bufferWriteData (ZBuffer buffer, const uint8_t* src, VkDeviceSize size);

template<template<class, class...> class C, class T, class... V>
VkDeviceSize	bufferWrite (
	ZBuffer buffer,
	const C<T, V...>& source,
	uint32_t dstIndex = 0u,
	uint32_t srcIndex = 0u,
	uint32_t count = INVALID_UINT32)
{
	extern VkDeviceSize bufferWriteData(
		ZBuffer buffer,
		const uint8_t * src,
		std::size_t elementSize,
		std::size_t	srcElementCount,
		uint32_t dstIndex,
		uint32_t srcIndex,
		uint32_t count);

	return bufferWriteData(buffer, reinterpret_cast<const uint8_t*>(source.data()),
							sizeof(T), source.size(), dstIndex, srcIndex, count);
}

template<class T>
VkDeviceSize   bufferWrite (ZBuffer buffer, const T & data, uint32_t dstIndex = 0u)
{
	extern VkDeviceSize bufferWriteData(
		ZBuffer buffer,
		const uint8_t * src,
		std::size_t elementSize,
		std::size_t	srcElementCount,
		uint32_t dstIndex,
		uint32_t srcIndex,
		uint32_t count);

	return bufferWriteData(buffer, static_cast<add_cptr<uint8_t>>(static_cast<add_cptr<void>>(&data)),
							sizeof(T), dstIndex + 1u, dstIndex, 0u, 1u);
}

template<class T>
VkDeviceSize	bufferFill (ZBuffer buffer, add_cref<T> value)
{
	std::vector<T> data(bufferGetElementCount<T>(buffer), value);
	return bufferWrite(buffer, data);
}

template<class T, std::size_t N>
void bufferRead (ZBuffer buffer, T (&table)[N])
{
	const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();
	const uint32_t		count		= uint32_t(bufferSize / sizeof(T));
	ASSERTMSG(count <= N, "Array size must accomodate all buffer data");
	const VkDeviceSize	readSize	= count * sizeof(T);
	bufferReadData(buffer, reinterpret_cast<add_ptr<uint8_t>>(&table[0]), readSize);
}

template<class T>
VkDeviceSize bufferRead (ZBuffer buffer, T& data)
{
	return bufferReadData(buffer, reinterpret_cast<add_ptr<uint8_t>>(&data), sizeof(T));
}

template<class T>
VkDeviceSize	bufferRead (ZBuffer buffer, std::vector<T>& container, uint32_t resizeElementCount = std::numeric_limits<uint32_t>::max())
{
	const VkDeviceSize	bufferSize		= buffer.getParam<VkDeviceSize>();
	const uint32_t		availableCount	= static_cast<uint32_t>(bufferSize / sizeof(T));
	if (container.size() == 0u)
	{
		resizeElementCount = std::clamp(resizeElementCount, 0u, availableCount);
		container.resize(resizeElementCount);
	}
	uint32_t readCount = std::clamp((uint32_t)container.size(), 0u, availableCount);
	const VkDeviceSize readSize = readCount * sizeof(T);
	return bufferReadData(buffer, reinterpret_cast<uint8_t*>(container.data()), readSize);
}

namespace namespace_hidden
{
struct BufferTexelAccess_
{
	Vec4 asColor (VkFormat format, uint32_t x, uint32_t y, uint32_t z = 0) const;

protected:
	BufferTexelAccess_ (ZBuffer buffer, uint32_t elementSize,
						uint32_t width, uint32_t height, uint32_t depth, uint32_t off, VkComponentTypeKHR componentType);
	virtual ~BufferTexelAccess_ ();
	add_ptr<void>	at (uint32_t x, uint32_t y, uint32_t z = 0);
	add_cptr<void>	at (uint32_t x, uint32_t y, uint32_t z = 0) const;

	ZBuffer						m_buffer;
	const uint32_t				m_elementSize;
	const VkComponentTypeKHR	m_componentType;
	const VkDeviceSize			m_bufferSize;
	const UVec3					m_size;
	const uint32_t              m_offset;
	add_ptr<uint8_t>			m_data;
};

template<class ElementType>
struct BufferTexelAccess_ComponentType
{
	static inline VkComponentTypeKHR type = ctype_to_vk_component_type<ElementType>;
};

template<class ElementType, size_t N>
struct BufferTexelAccess_ComponentType<VecX<ElementType, N>>
{
	static inline VkComponentTypeKHR type = ctype_to_vk_component_type<ElementType>;
};

} // namespace_hidden

template<class ElementType>
struct BufferTexelAccess : namespace_hidden::BufferTexelAccess_
{
	BufferTexelAccess (ZBuffer buffer, uint32_t width, uint32_t height, uint32_t depth = 1u, uint32_t startElement = 0u)
		: BufferTexelAccess_ (buffer, static_cast<uint32_t>(sizeof(ElementType)), width, height, depth, startElement,
							  namespace_hidden::BufferTexelAccess_ComponentType<ElementType>::type) {}
	add_ref<ElementType>	at (uint32_t x, uint32_t y, uint32_t z = 0)
	{
		return *static_cast<add_ptr<ElementType>>(BufferTexelAccess_::at(x, y, z));
	}
	add_cref<ElementType>	at (uint32_t x, uint32_t y, uint32_t z = 0) const
	{
		return *static_cast<add_cptr<ElementType>>(BufferTexelAccess_::at(x, y, z));
	}
	auto stdBeginEnd () { return makeStdBeginEnd<ElementType>(m_data, (m_size.x() * m_size.y() * m_size.z())); }
	uint32_t getElementCount() const { return m_size.prod(); }
};

} // namespace vtf

#endif // __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
