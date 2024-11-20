#include "vtfPlatformDriver.hpp"

#include <dlfcn.h>
#include <link.h>

#include <atomic>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <type_traits>
#include <vector>

namespace fs = std::filesystem;

extern bool getVtfVerboseMode();
extern const std::string& getVtfCustomDriver();
extern bool compareNoCase(const std::string& a, const std::string& b);

//static_assert(std::is_same_v<decltype(*(&dlclose)), int (&)(void*) noexcept>, "???");
//std::cout << demangledName(&dlclose) << std::endl;

bool verifyVulkanDriver (void* h)
{
    return (h != nullptr) && dlsym(h, "vkCreateInstance");
}

#ifdef VULKAN_CUSTOM_DRIVER

typedef void* (*dlopen_t)(const char*, int);
static std::atomic<void*> __customDriver;
static std::atomic<dlopen_t> __system_dlopen;

std::string readLibraryPath(void* lib);

#if 0
std::map<std::string, void*> __libraries;
std::recursive_mutex __libraries_mutex;
#endif

extern "C" void* my_dlopen(const char* libFileName, int flags) __attribute__((alias("dlopen")));

void* dlopen(const char* libFileName, int flags) 
{
    const bool verboseMode(getVtfVerboseMode());
    const std::string vulkanDriverFileName(fs::path(VULKAN_DRIVER).filename());
    const std::string currentLibFileName(libFileName
                                    ? fs::path(libFileName).filename().string()
                                    : std::string());
    const std::string customVtfDriver = getVtfCustomDriver().empty()
                                            ? std::string()
                                            : fs::absolute(getVtfCustomDriver()).string();
    auto readDlError = []() -> const char*
    {
        const char* error = dlerror();
        return error ? error : "<unrecognized>";
    };

    int qqCounter = 0;
    auto qq = [&qqCounter](const std::string& qqText = std::string()) -> void
    {
        std::cout << "QQ " << (qqCounter++) << ' ' << qqText << std::endl;
    };
    static_cast<void>(qqCounter);
    static_cast<void>(qq);

#if 0
    static int libCounter;
    if (libFileName)
    {
        std::cout << "Process: " << ++libCounter << ' ' << libFileName << std::endl;
    }
#endif

    dlopen_t this_dlopen, sys_dlopen;
    dlopen_t null_dlopen = nullptr;
    *(void**)(&sys_dlopen) = dlsym(RTLD_NEXT, "dlopen");
    *(void**)(&this_dlopen) = dlsym(RTLD_DEFAULT, "dlopen");
    if (this_dlopen != sys_dlopen)
    {
        __system_dlopen.compare_exchange_strong(null_dlopen, sys_dlopen, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void* (*sys_dlsym)(int, const char*) = nullptr;
    *(void**)(&sys_dlsym) = dlsym(RTLD_DEFAULT, "dlsym");

    if ((getVtfCustomDriver().empty() == false) && compareNoCase(vulkanDriverFileName, currentLibFileName))
    {
        if (verboseMode)
        {
            std::cout << "[DRIVER] system_dlopen           " << *(void**)&__system_dlopen << std::endl;
            std::cout << "         system_dlsym            " << *(void**)&sys_dlsym << std::endl;
            std::cout << "         local_dlopen            " << *(void**)&this_dlopen << std::endl;
            std::cout << "         Handled loading library " << std::quoted(libFileName) << std::endl;
            std::cout << "         Trying to load driver   " << customVtfDriver << std::endl;
        }

        void* oldDriver = (*__system_dlopen.load(std::memory_order_acquire))(currentLibFileName.c_str(), RTLD_NOLOAD | RTLD_LAZY);
        void* newDriver = (*__system_dlopen.load(std::memory_order_acquire))(customVtfDriver.c_str(), RTLD_LAZY);

        if (newDriver)
        {
            if (verifyVulkanDriver(newDriver))
            {
                if (verboseMode)
                {
                    std::cout << "[DRIVER] Successfully loaded driver library" << std::endl;
                }

                if (oldDriver)
                {
                    const std::string oldDriverPath(verboseMode ? readLibraryPath(oldDriver) : std::string());
                    if (dlclose(oldDriver) == 0)
                    {
                        if (verboseMode)
                        {
                            //const std::string oldDriverFile(fs::path(oldDriverPath) / currentLibFileName);
                            std::cout << "[DRIVER] Successfully unloaded old driver library, was "
                                      << std::quoted(oldDriverPath) << std::endl;
                        }
                    }
                    else if (verboseMode)
                    {
                        std::cout << "[DRIVER] Failed to unload old driver library, error: " << readDlError() << std::endl;
                    }
                }

                __customDriver.store(newDriver, std::memory_order_relaxed);

                if (verboseMode)
                {
                    std::cout << "[DRIVER] Assign new driver handle: " << newDriver << std::endl;
                }

                return newDriver;
            }
            else if (verboseMode)
            {
                std::cout << "[DRIVER] Given " << std::quoted(customVtfDriver) << " is not valid Vulkan Driver\n"
                          << "          Instead trying to load " << std::quoted(libFileName) << std::endl;
            }
        }
        else if (verboseMode)
        {
            std::cout << "[DRIVER] Driver library loading failed: " << readDlError() << std::endl
                      << "          Instead trying to load " << std::quoted(libFileName) << std::endl;
        }
    }

#if 0
    static int unique;
    if (libFileName)
    {
        std::lock_guard<std::recursive_mutex> lock(__libraries_mutex);

        void* lib = nullptr;
	const std::string strLib(libFileName);
        auto ilib = __libraries.find(strLib);
	if (ilib == __libraries.end())
	{
            std::cout << "Unique: " << ++unique << ' ' << libFileName << std::endl;
            lib = (*__system_dlopen.load(std::memory_order_acquire))(libFileName, flags);
	    __libraries[strLib] = lib;
	}
	else lib = ilib->second;
        return lib;
    }
#endif

    return (*__system_dlopen.load(std::memory_order_acquire))(libFileName, flags);
}
#endif // VULKAN_CUSTOM_DRIVER

DriverInitializer::DriverInitializer ()
{
}

DriverInitializer::~DriverInitializer ()
{
}

auto DriverInitializer::isCustomDriver() -> bool
{
#ifdef VULKAN_CUSTOM_DRIVER
    return (nullptr != __customDriver.load(std::memory_order_acquire));
#else
    return false;
#endif
}

std::string readLibraryPath(void* lib)
{
    link_map* map = nullptr;

    if ((dlinfo(lib, RTLD_DI_LINKMAP, &map) != 0) || (nullptr == map))
    {
        throw std::runtime_error(dlerror());
    }

    return map->l_name;
}

auto DriverInitializer::getPlatformDriverFileName(bool& success) -> std::string
{
    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__ << ": " << std::boolalpha << isCustomDriver() << std::noboolalpha << std::endl;
    }

#ifdef VULKAN_CUSTOM_DRIVER
    const std::string path(isCustomDriver()
                          ? readLibraryPath(__customDriver.load(std::memory_order_acquire))
                          : VULKAN_DRIVER);
#else
    const std::string path(readLibraryPath(dlopen(VULKAN_DRIVER, RTLD_NOLOAD|RTLD_LAZY)));
#endif
    success = (path.empty() == false);

    return fs::absolute(path);
}

auto DriverInitializer::getPlatformDriverProc(const char* procName) -> std::add_pointer_t<void>
{
    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__ << ": " << std::boolalpha << isCustomDriver() << std::noboolalpha << std::endl;
    }

#ifdef VULKAN_CUSTOM_DRIVER
    void* handle = isCustomDriver()
                       ? __customDriver.load(std::memory_order_acquire)
                       : (__system_dlopen.load(std::memory_order_acquire)
                              ? (*__system_dlopen.load(std::memory_order_acquire))(VULKAN_DRIVER, RTLD_NOLOAD | RTLD_LAZY)
                              : dlopen(VULKAN_DRIVER, RTLD_NOLOAD | RTLD_LAZY));
#else
    void* handle = dlopen(VULKAN_DRIVER, RTLD_NOLOAD | RTLD_LAZY);
#endif

    return handle ? dlsym(handle, procName) : nullptr;
}

