#ifndef __VTF_ZDEVICEMEMORY_HPP_INCLUDED__
#define __VTF_ZDEVICEMEMORY_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"

namespace vtf
{

uint32_t		findMemoryTypeIndex	(ZDevice device, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
ZDeviceMemory	createMemory		(ZDevice device, const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties);
uint8_t*		mapMemory			(ZDeviceMemory memory);
void			unmapMemory			(ZDeviceMemory memory);
void			flushMemory			(ZDeviceMemory memory);
void			invalidateMemory	(ZDeviceMemory memory);

} // namespace vtf

#endif // __VTF_ZDEVICEMEMORY_HPP_INCLUDED__
