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

protected:
    friend auto getPlatformDriverProc(const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>;
};

#endif // __VTF_PLATFORM_DRIVER_HPP_INCLUDED__
