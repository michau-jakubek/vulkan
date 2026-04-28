#ifndef __VK_DEBUG_MESSENGER_HPP_INCLUDED__
#define __VK_DEBUG_MESSENGER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"

namespace vtf
{
void makeDebugCreateInfo    (VkDebugUtilsMessengerCreateInfoEXT& result, void* pUserData, void* pNext = nullptr, bool enableDebugPrintf = false);
void makeDebugCreateInfo	(VkDebugReportCallbackCreateInfoEXT& result, void* pUserData, void* pNext = nullptr, bool enableDebugPrintf = false);
void createDebugMessenger	(add_cref<ZInstanceInterface>, VkInstance, VkAllocationCallbacksPtr, const VkDebugUtilsMessengerCreateInfoEXT&, VkDebugUtilsMessengerEXT&);
void destroyDebugMessenger	(add_cref<ZInstanceInterface>, VkInstance, VkAllocationCallbacksPtr, add_ref<VkDebugUtilsMessengerEXT>);
void createDebugReport		(add_cref<ZInstanceInterface>, VkInstance, VkAllocationCallbacksPtr, const VkDebugReportCallbackCreateInfoEXT&, VkDebugReportCallbackEXT&);
void destroyDebugReport		(add_cref<ZInstanceInterface>, VkInstance, VkAllocationCallbacksPtr, add_ref<VkDebugReportCallbackEXT>);

} // namespace vtf

#endif // __VK_DEBUG_MESSENGER_HPP_INCLUDED__
