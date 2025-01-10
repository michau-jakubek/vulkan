#include "vtfBacktrace.hpp"
#include "vtfFilesystem.hpp"
#include "vtfVulkanDriver.hpp"
#include "vtfPlatformDriver.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>

#define EXTERN_IN_PDRIVER_MODULE_BEGIN
bool getVtfVerboseMode ()
{
    return getGlobalAppFlags().verbose;
}

const std::string& getVtfCustomDriver ()
{
    return getGlobalAppFlags().vulkanDriver;
}

bool compareNoCase(const std::string& a, const std::string& b)
{
    return vtf::compareNoCase(a, b);
}
#define EXTERN_IN_PDRIVER_MODULE_END

std::string getDriverFileName (add_ref<bool> success)
{
    return DriverInitializer::getPlatformDriverFileName(success);
}

PFN_vkCreateInstance getDriverCreateInstanceProc ()
{
    return (PFN_vkCreateInstance)DriverInitializer::getPlatformDriverProc("vkCreateInstance");
}

PFN_vkDestroyInstance getDriverDestroyInstanceProc()
{
    return (PFN_vkDestroyInstance)DriverInitializer::getPlatformDriverProc("vkDestroyInstance");
}

PFN_vkCreateDevice getDriverCreateDeviceProc ()
{
    return (PFN_vkCreateDevice)DriverInitializer::getPlatformDriverProc("vkCreateDevice");
}

PFN_vkDestroyDevice getDriverDestroyDeviceProc()
{
    return (PFN_vkDestroyDevice)DriverInitializer::getPlatformDriverProc("vkDestroyDevice");
}

PFN_vkGetInstanceProcAddr getDriverGetInstanceProcAddr ()
{
    return (PFN_vkGetInstanceProcAddr)DriverInitializer::getPlatformDriverProc("vkGetInstanceProcAddr");
}

PFN_vkGetDeviceProcAddr getDriverGetDeviceProcAddr ()
{
    return (PFN_vkGetDeviceProcAddr)DriverInitializer::getPlatformDriverProc("vkGetDeviceProcAddr");
}

PFN_vkEnumerateInstanceLayerProperties getDriverEnumerateInstanceLayerPropertiesProcAddr ()
{
    return (PFN_vkEnumerateInstanceLayerProperties)
        DriverInitializer::getPlatformDriverProc("vkEnumerateInstanceLayerProperties");
}

PFN_vkEnumerateInstanceExtensionProperties getDriverEnumerateInstanceExtensionPropertiesProcAddr ()
{
    return (PFN_vkEnumerateInstanceExtensionProperties)
        DriverInitializer::getPlatformDriverProc("vkEnumerateInstanceExtensionProperties");
}