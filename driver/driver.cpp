#include "vtfBacktrace.hpp"
#include "driver.hpp"

#ifdef _MSC_VER
#include <windows.h>
#define DELAYIMP_INSECURE_WRITABLE_HOOKS
#include <delayimp.h>
#endif

#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _MSC_VER

bool verifyVulkanDriver (HMODULE h)
{
    return GetProcAddress(h, "vkCreateInstance");
}

extern "C" FARPROC WINAPI DliNotifyHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
    switch (dliNotify) {
    case dliNotePreLoadLibrary:
        if (!gf.vulkanDriver.empty() && vtf::compareNoCase(VULKAN_DRIVER, pdli->szDll))
        {
            if (gf.verbose)
            {
                std::cout << "[INFO] Handled loading library " << std::quoted(pdli->szDll) << std::endl;
                std::cout << "       Trying to load driver   " << gf.vulkanDriver << std::endl;
            }
            const auto oldDriver = GetModuleHandle(pdli->szDll);
            const auto newDriver = LoadLibrary(gf.vulkanDriver.c_str());

            if (oldDriver)
            {
                if (FreeLibrary(oldDriver))
                {
                    std::vector<TCHAR> p(1024);
                    GetModuleFileName(oldDriver, p.data(), (DWORD)p.size());
                    if (gf.verbose)
                    {
                        std::cout << "[INFO] Successfully unloaded old driver library, was "
                            << std::quoted(p.data()) << std::endl;
                    }
                }
                else if (gf.verbose)
                {
                    const auto ec = GetLastError();
                    std::cout << "[INFO] Failed to unload old driver library, error: " << ec << std::endl;
                }
            }

            if (newDriver)
            {
                if (verifyVulkanDriver(newDriver))
                {
                    if (gf.verbose)
                    {
                        std::cout << "[INFO] Successfully loaded driver library" << std::endl;
                    }
                    return (FARPROC)newDriver;
                }
                else if (gf.verbose)
                {
                    std::cout << "[WARNING] Given " << std::quoted(gf.vulkanDriver) << " is not valid Vulkan Driver\n"
                              << "          Instead trying to load " << std::quoted(pdli->szDll) << std::endl;
                }
            }
            else if (gf.verbose)
            {
                std::cout << "[WARNING] Driver library loading failed\n"
                          << "          Instead trying to load " << std::quoted(pdli->szDll) << std::endl;
            }
        }
        else if (gf.verbose)
        {
            std::cout << "[INFO] Handled loading library " << std::quoted(pdli->szDll) << std::endl;
            std::cout << "       Vulkan driver option nor VULKAN_DRIVER not specified" << std::endl;
        }
        break;
    }
    return NULL;
}
PfnDliHook __pfnDliNotifyHook2 = DliNotifyHook;
#endif // _MSC_VER

DriverInitializer::DriverInitializer ()
{
#ifdef _MSC_VER
    __pfnDliNotifyHook2 = DliNotifyHook;
#endif
}

std::string getDriverFileName (add_ref<bool> success)
{
    std::string name;
#ifdef _MSC_VER
    const std::string driverFiles[]
    {
        VULKAN_DRIVER,
        getGlobalAppFlags().vulkanDriver.empty()
            ? std::string()
            : fs::path(getGlobalAppFlags().vulkanDriver).filename().string()
    };
    for (add_cref<std::string> driverFile : driverFiles)
    {
        const HMODULE h = GetModuleHandle(driverFile.c_str());
        if (success = !driverFile.empty() && h != nullptr; success)
        {
            std::vector<TCHAR> s(1024);
            GetModuleFileName(h, s.data(), (DWORD)(s.size() - 1));
            name.assign(s.data());
            break;
        }
    }
#else
	std::ostringstream os;
	os << "(experimental) " << __func__ << " in " << __FILE__ << " at " << __LINE__;
	os.flush();
	name = os.str();
#endif
    return name;
}
