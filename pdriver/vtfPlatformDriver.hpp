#ifndef __VTF_PLATFORM_DRIVER_HPP_INCLUDED__
#define __VTF_PLATFORM_DRIVER_HPP_INCLUDED__

#include <cstdint>
#include <string>
#include <type_traits>

struct DriverInitializer
{
    DriverInitializer ();
    ~DriverInitializer ();

    void operator ()(const std::string& customVtfDriver, uint32_t verboseMode);

    static auto isCustomDriver() -> bool;
    static auto getPlatformDriverFileName(bool& success) -> std::string;
    static auto getPlatformDriverProc(const char* procName) -> std::add_pointer_t<void>;

    template<class Signature>
    static Signature getPlatformDriverProc(Signature, const char* procName)
    {
        return reinterpret_cast<Signature>(getPlatformDriverProc(procName));
    }
    template<class Signature, class DriverProc, class Device>
    static Signature getPlatformDeviceProc(Signature, DriverProc dp, Device device, const char* procName)
    {
        auto pfnVkGetDeviceProcAddr = getPlatformDriverProc(dp, "vkGetDeviceProcAddr");
        return (Signature)((*pfnVkGetDeviceProcAddr)(device, procName));
    }
    template<class Signature, class DriverProc, class Instance>
    static Signature getPlatformInstanceProc(Signature, DriverProc dp, Instance instance, const char* procName)
    {
        auto pfnVkGetInstanceProcAddr = getPlatformDriverProc(dp, "vkGetInstanceProcAddr");
        return (Signature)((*pfnVkGetInstanceProcAddr)(instance, procName));
    }

protected:
    friend auto getPlatformDriverProc(const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>;
};

#endif // __VTF_PLATFORM_DRIVER_HPP_INCLUDED__
