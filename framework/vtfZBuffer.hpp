#ifndef __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
#define __VTF_ZBUFFER_UTILS_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeviceMemory.hpp"

namespace vtf
{

int				computePixelByteWidth (VkFormat format);
int				computePixelChannelCount (VkFormat format);
uint32_t		sampleFlagsToSampleCount(VkSampleCountFlags flags);
uint32_t		computeMipLevelCount (uint32_t width, uint32_t height);
VkDeviceSize	computeBufferSize (VkFormat format, uint32_t baseLevelWidth, uint32_t baseLevelHeight, uint32_t baseLevel, uint32_t levels, uint32_t layers, VkSampleCountFlags samples);

ZBuffer			createBuffer	(ZDevice device, VkDeviceSize size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);
ZBuffer			createBuffer	(ZDevice device, VkFormat format, uint32_t elements, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);
ZBuffer			createBuffer	(ZDevice device, ZImageView view, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties);
ZBuffer			createBuffer	(ZDevice device, ZImage image, ZBufferUsageFlags usage, uint32_t baseLevel, uint32_t levels, ZMemoryPropertyFlags properties);

void			flushBuffer		(ZBuffer buffer);
uint32_t		writeBufferData	(ZBuffer buffer, const uint8_t* src, const VkBufferCopy& copy, bool flush = true);
uint32_t		readBufferData	(ZBuffer buffer, uint8_t* dst, const VkBufferCopy& copy);
uint32_t		writeBufferData	(ZBuffer buffer, const uint8_t* src, VkDeviceSize size = VK_WHOLE_SIZE);
uint32_t		readBufferData	(ZBuffer buffer, uint8_t* dst, VkDeviceSize size = VK_WHOLE_SIZE);

template<template<class, class...> class C, class T, class... V>
bool writeBuffer(ZBuffer buffer, const C<T, V...>& c, uint32_t count = INVALID_UINT32)
{
	if (count == INVALID_UINT32 || count > c.size()) count = c.size();
	const VkDeviceSize	dataSize	= count * sizeof(T);
	const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();
	const VkDeviceSize	writeSize	= writeBufferData(buffer, reinterpret_cast<const uint8_t*>(c.data()), dataSize);
	return (writeSize < bufferSize);
}

template<class T, uint32_t N>
bool writeBuffer(ZBuffer buffer, T const (&table)[N])
{
	const VkDeviceSize	dataSize	= N * sizeof(T);
	const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();
	const VkDeviceSize	writeSize	= writeBufferData(buffer, table, dataSize);
	return (writeSize < bufferSize);
}

template<class T, class C = std::vector<T>>
C readBuffer(ZBuffer buffer, uint32_t count = INVALID_UINT32)
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
uint32_t readBuffer(ZBuffer buffer, std::vector<T>& container, uint32_t count = INVALID_UINT32)
{
	const VkDeviceSize	bufferSize		= buffer.getParam<VkDeviceSize>();
	const uint32_t		availableCount	= bufferSize / sizeof(T);
	if (count == INVALID_UINT32 || count > availableCount)
		count = availableCount;
	const VkDeviceSize	readSize		= count * sizeof(T);
	container.resize(count);
	return readBufferData(buffer, reinterpret_cast<uint8_t*>(container.data()), readSize);
}

} // namespace vtf

#endif // __VTF_ZBUFFER_UTILS_HPP_INCLUDED__
