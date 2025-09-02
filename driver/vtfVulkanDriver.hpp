#ifndef __VTF_VULKAN_DRIVER_HPP_INCLUDED__
#define __VTF_VULKAN_DRIVER_HPP_INCLUDED__

#include "vtfCUtils.hpp"

std::string									getDriverFileName (add_ref<bool> success);
PFN_vkCreateInstance						getDriverCreateInstanceProc  ();
PFN_vkDestroyInstance						getDriverDestroyInstanceProc ();
PFN_vkCreateDevice							getDriverCreateDeviceProc    ();
PFN_vkCreateDevice							getInstanceCreateDeviceProc	 (VkInstance);
PFN_vkDestroyDevice							getDriverDestroyDeviceProc   ();
PFN_vkGetInstanceProcAddr					getDriverGetInstanceProcAddr ();
PFN_vkGetDeviceProcAddr						getDriverGetDeviceProcAddr   ();
PFN_vkEnumerateInstanceLayerProperties		getDriverEnumerateInstanceLayerPropertiesProcAddr ();
PFN_vkEnumerateInstanceExtensionProperties	getDriverEnumerateInstanceExtensionPropertiesProcAddr ();

#endif // __VTF_VULKAN_DRIVER_HPP_INCLUDED__
