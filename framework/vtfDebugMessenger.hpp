#ifndef __VK_DEBUG_MESSENGER_HPP_INCLUDED__
#define __VK_DEBUG_MESSENGER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{
void			makeDebugCreateInfo		(VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext = nullptr, bool enableDebugPrintf = false);
void			makeDebugCreateInfo		(VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext = nullptr, bool enableDebugPrintf = false);
void			createDebugMessenger	(ZInstance, VkAllocationCallbacksPtr, const VkDebugUtilsMessengerCreateInfoEXT&, VkDebugUtilsMessengerEXT&);
void			destroyDebugMessenger	(ZInstance, VkAllocationCallbacksPtr, VkDebugUtilsMessengerEXT&);
void			createDebugReport		(ZInstance, VkAllocationCallbacksPtr, const VkDebugReportCallbackCreateInfoEXT&, VkDebugReportCallbackEXT&);
void			destroyDebugReport		(ZInstance, VkAllocationCallbacksPtr, VkDebugReportCallbackEXT&);

} // namespace vtf

#endif // __VK_DEBUG_MESSENGER_HPP_INCLUDED__
