#ifndef __VTF_VULKAN_DRIVER_HPP_INCLUDED__
#define __VTF_VULKAN_DRIVER_HPP_INCLUDED__

#include "vtfCUtils.hpp"

std::string					getDriverFileName (add_ref<bool> success);
PFN_vkCreateInstance		getDriverCreateInstanceProc  ();
PFN_vkCreateDevice			getDriverCreateDeviceProc    ();
PFN_vkGetInstanceProcAddr	getDriverGetInstanceProcAddr ();
PFN_vkGetDeviceProcAddr		getDriverGetDeviceProcAddr   ();

#endif // __VTF_VULKAN_DRIVER_HPP_INCLUDED__
