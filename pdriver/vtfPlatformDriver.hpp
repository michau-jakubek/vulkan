#ifndef __VTF_PLATFORM_DRIVER_HPP_INCLUDED__
#define __VTF_PLATFORM_DRIVER_HPP_INCLUDED__

#include <cstdint>
#include <string>
#include <type_traits>

struct DriverInitializer
{
    DriverInitializer ();
    ~DriverInitializer ();

    void operator ()(const std::string& customVkDriver, uint32_t verboseMode);

    static auto getPlatformDriverFileName(bool& success) -> std::string;
    static auto getPlatformDriverProc(const char* procName) -> std::add_pointer_t<void>;

    template<class Signature>
    static Signature getPlatformDriverProc(Signature, const char* procName)
    {
        return reinterpret_cast<Signature>(getPlatformDriverProc(procName));
    }
    template<class Signature, class GetDeviceProcAddr, class Device>
    static Signature getPlatformDeviceProc(Signature, GetDeviceProcAddr dpa, Device device, const char* procName)
    {
        auto pfnVkGetDeviceProcAddr = getPlatformDriverProc(dpa, "vkGetDeviceProcAddr");
        return (Signature)((*pfnVkGetDeviceProcAddr)(device, procName));
    }
    template<class Signature, class GetInstanceProcAddr, class Instance>
    static Signature getPlatformInstanceProc(Signature, GetInstanceProcAddr ipa, Instance instance, const char* procName)
    {
        auto pfnVkGetInstanceProcAddr = getPlatformDriverProc(ipa, "vkGetInstanceProcAddr");
        return (Signature)((*pfnVkGetInstanceProcAddr)(instance, procName));
    }

protected:
    friend auto getPlatformDriverProc(const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>;
};

#endif // __VTF_PLATFORM_DRIVER_HPP_INCLUDED__
