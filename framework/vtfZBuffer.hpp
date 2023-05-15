#ifndef __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
#define __VTF_ZBUFFER_UTILS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeviceMemory.hpp"

namespace vtf
{


uint32_t		computeBufferPixelCount (ZImage image);
uint32_t		computeBufferPixelCount (ZImageView image);
VkDeviceSize	computeBufferSize		(ZImage image);
VkDeviceSize	computeBufferSize		(ZImageView view);
VkDeviceSize	computeBufferSize		(VkFormat format, uint32_t imageWidth, uint32_t imageHeight,
										 uint32_t baseLevel = 0u, uint32_t levels = 1u, uint32_t layers = 1u);
auto			makeBufferMemoryBarrier	(ZBuffer buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkDeviceSize size = INVALID_UINT64) -> VkBufferMemoryBarrier;
//void			copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImage image,
//								   uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32,
//								   VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL);
//void			copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImageView view, VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL);
void			copyBufferToBuffer (ZCommandBuffer cmd, ZBuffer src, ZBuffer dst, const VkBufferCopy& region);

ZBuffer			createBuffer	(ZDevice device, const ExplicitWrapper<VkDeviceSize>& size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);
ZBuffer			createBuffer	(ZDevice device, VkFormat format, uint32_t elements, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);
ZBuffer			createBuffer	(ZImage image, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);

void			flushBuffer		(ZBuffer buffer);
uint32_t		writeBufferData	(ZBuffer buffer, const uint8_t* src, const VkBufferCopy& copy, bool flush = true);
uint32_t		readBufferData	(ZBuffer buffer, uint8_t* dst, const VkBufferCopy& copy);
uint32_t		writeBufferData	(ZBuffer buffer, const uint8_t* src, VkDeviceSize size = VK_WHOLE_SIZE);
uint32_t		readBufferData	(ZBuffer buffer, uint8_t* dst, VkDeviceSize size = VK_WHOLE_SIZE);

template<template<class, class...> class C, class T, class... V>
uint32_t writeBuffer (ZBuffer buffer, const C<T, V...>& c, uint32_t count = INVALID_UINT32)
{
	if (count == INVALID_UINT32 || count > c.size()) count = static_cast<uint32_t>(c.size());
	const VkDeviceSize	dataSize	= count * sizeof(T);
	const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();
	return writeBufferData(buffer, reinterpret_cast<const uint8_t*>(c.data()), std::min(dataSize, bufferSize));
}

template<class T, uint32_t N>
uint32_t writeBuffer (ZBuffer buffer, T const (&table)[N])
{
	const VkDeviceSize	dataSize	= N * sizeof(T);
	const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();
	return writeBufferData(buffer, table, std::min(dataSize, bufferSize));
}

template<class T, class C = std::vector<T>>
C readBuffer (ZBuffer buffer, uint32_t count = INVALID_UINT32)
{
	const VkDeviceSize	bufferSize		= buffer.getParam<VkDeviceSize>();
	const uint32_t		availableCount	= bufferSize / sizeof(T);
	if (count == INVALID_UINT32 || count > availableCount)
		count = availableCount;
	const VkDeviceSize	readSize		= count * sizeof(T);
	C container(count);
	readBufferData(buffer, reinterpret_cast<uint8_t*>(container.data()), readSize);
	return container;
}

template<class T>
uint32_t readBuffer (ZBuffer buffer, std::vector<T>& container, uint32_t vectorElementCount = INVALID_UINT32)
{
	const VkDeviceSize	bufferSize		= buffer.getParam<VkDeviceSize>();
	const uint32_t		availableCount	= static_cast<uint32_t>(bufferSize / sizeof(T));
	if (vectorElementCount == INVALID_UINT32 || vectorElementCount > availableCount)
		vectorElementCount = availableCount;
	const VkDeviceSize	readSize		= vectorElementCount * sizeof(T);
	container.resize(vectorElementCount);
	return readBufferData(buffer, reinterpret_cast<uint8_t*>(container.data()), readSize);
}

} // namespace vtf

#endif // __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
