#include "vtfPlatformDriver.hpp"

#include <windows.h>
#ifdef VULKAN_CUSTOM_DRIVER
  #define DELAYIMP_INSECURE_WRITABLE_HOOKS
  #include <delayimp.h>
#endif

#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

extern bool getVtfVerboseMode ();
extern const std::string& getVtfCustomDriver ();
extern bool compareNoCase (const std::string& a, const std::string& b);

bool verifyVulkanDriver (HMODULE h)
{
    return GetProcAddress(h, "vkCreateInstance");
}

#ifdef VULKAN_CUSTOM_DRIVER

static FARPROC __customDriver;

extern "C" FARPROC WINAPI DliNotifyHook (unsigned dliNotify, PDelayLoadInfo pdli)
{
    const bool verboseMode(getVtfVerboseMode());
    const std::string customVtfDriver(getVtfCustomDriver());

    switch (dliNotify) {
    case dliNotePreLoadLibrary:
        if (!customVtfDriver.empty() && compareNoCase(VULKAN_DRIVER, pdli->szDll))
        {
            if (verboseMode)
            {
                std::cout << "[DRIVER] Handled loading library " << std::quoted(pdli->szDll) << std::endl;
                std::cout << "         Trying to load driver   " << customVtfDriver << std::endl;
            }
            const auto oldDriver = GetModuleHandle(pdli->szDll);
            const auto newDriver = LoadLibrary(customVtfDriver.c_str());

            if (oldDriver)
            {
                std::vector<TCHAR> p(1024);
                GetModuleFileName(oldDriver, p.data(), (DWORD)p.size());

                if (FreeLibrary(oldDriver))
                {
                    if (verboseMode)
                    {
                        std::cout << "[DRIVER] Successfully unloaded old driver library, was "
                                  << std::quoted(p.data()) << std::endl;
                    }
                }
                else if (verboseMode)
                {
                    const auto ec = GetLastError();
                    std::cout << "[DRIVER] Failed to unload old driver library, error: " << ec << std::endl;
                }
            }

            if (newDriver)
            {
                if (verifyVulkanDriver(newDriver))
                {
                    if (verboseMode)
                    {
                        std::cout << "[DRIVER] Successfully loaded driver library" << std::endl;
                    }
                    __customDriver = (FARPROC)newDriver;
                    return (FARPROC)newDriver;
                }
                else if (verboseMode)
                {
                    std::cout << "[DRIVER] Given " << std::quoted(customVtfDriver) << " is not valid Vulkan Driver\n"
                              << "         Instead trying to load " << std::quoted(pdli->szDll) << std::endl;
                }
            }
            else if (verboseMode)
            {
                std::cout << "[DRIVER] Driver library loading failed\n"
                          << "         Instead trying to load " << std::quoted(pdli->szDll) << std::endl;
            }
        }
        else if (verboseMode)
        {
            std::cout << "[DRIVER] Handled loading library " << std::quoted(pdli->szDll) << std::endl;
            std::cout << "         Vulkan driver option nor VULKAN_DRIVER not specified" << std::endl;
        }
        break;
    }
    return NULL;
}
PfnDliHook __pfnDliNotifyHook2 = DliNotifyHook;

#endif // VULKAN_CUSTOM_DRIVER

DriverInitializer::DriverInitializer ()
{
#ifdef VULKAN_CUSTOM_DRIVER
    __pfnDliNotifyHook2 = DliNotifyHook;
#endif
}
DriverInitializer::~DriverInitializer ()
{
}

auto DriverInitializer::isCustomDriver() -> bool
{
#ifdef VULKAN_CUSTOM_DRIVER
    return !!__customDriver;
#else
    return false;
#endif
}

auto DriverInitializer::getPlatformDriverFileName (bool& success) -> std::string
{
    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__ << ": " << std::boolalpha << isCustomDriver() << std::noboolalpha << std::endl;
    }

    std::string name;
    std::vector<TCHAR> p(1024);
#ifdef VULKAN_CUSTOM_DRIVER
    const HMODULE handle = isCustomDriver()
                        ? (HMODULE)__customDriver
                        : GetModuleHandle(fs::path(VULKAN_DRIVER).filename().string().c_str());
#else
    const HMODULE handle = GetModuleHandle(fs::path(VULKAN_DRIVER).filename().string().c_str());
#endif

    if (success = (handle != nullptr); success)
    {
        GetModuleFileName(handle, p.data(), (DWORD)p.size());
        name.assign(p.data());
    }

    return name;
}

auto DriverInitializer::getPlatformDriverProc (const char* procName) -> std::add_pointer_t<void>
{
    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__ << ": " << std::boolalpha << isCustomDriver() << std::noboolalpha << std::endl;
    }

#ifdef VULKAN_CUSTOM_DRIVER
    HMODULE driver = isCustomDriver()
        ? (HMODULE)__customDriver
        : GetModuleHandle(fs::path(VULKAN_DRIVER).filename().string().c_str());
#else
    const HMODULE driver = GetModuleHandle(fs::path(VULKAN_DRIVER).filename().string().c_str());
#endif

    return driver ? GetProcAddress(driver, procName) : nullptr;
}

