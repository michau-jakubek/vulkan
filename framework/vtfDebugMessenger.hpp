#ifndef __VK_DEBUG_MESSENGER_HPP_INCLUDED__
#define __VK_DEBUG_MESSENGER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{
void			getDebugCreateInfo		(VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext = nullptr);
void			getDebugCreateInfo		(VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext = nullptr);
void			createDebugMessenger	(ZInstance, VkAllocationCallbacksPtr, void* pUserData, VkDebugUtilsMessengerEXT&);
void			destroyDebugMessenger	(ZInstance, VkAllocationCallbacksPtr, VkDebugUtilsMessengerEXT&);
void			createDebugReport		(ZInstance, VkAllocationCallbacksPtr, void* pUserData, VkDebugReportCallbackEXT&);
void			destroyDebugReport		(ZInstance, VkAllocationCallbacksPtr, VkDebugReportCallbackEXT&);

} // namespace vtf

#endif // __VK_DEBUG_MESSENGER_HPP_INCLUDED__
