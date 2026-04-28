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

extern bool getVtfVerboseMode ();
extern bool compareNoCase (const std::string& a, const std::string& b);

static std::string readLibraryPath (void* lib);

auto getPlatformDriverProc (const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>;

bool verifyVulkanDriver(void* h)
{
    bool icd = false;
    return !!getPlatformDriverProc((const char*)"vkCreateInstance", h, icd);
}

#ifdef VULKAN_CUSTOM_DRIVER

typedef void* (*dlopen_t)(const char*, int);
static std::atomic<void*> __customDriver;
static std::atomic<dlopen_t> __system_dlopen;
static bool __verboseMode;
static std::string __customVtfDriver;
static const std::string __vulkanDriverFileName = fs::path(VULKAN_DRIVER).filename();

static void* system_dlopen (const char* lib, int flags)
{
    return (*__system_dlopen.load(std::memory_order_acquire))(lib, flags);
}
static void* system_dlopen (const std::string& lib, int flags)
{
    return system_dlopen(lib.c_str(), flags);
}

static void freeDriver (const char* driver, bool verboseMode);
static void freeDriver (const std::string& driver, bool verboseMode);

static void freeDriver (void* driver, bool verboseMode)
{
    if (!driver) return;

    const std::string driverPath(verboseMode ? readLibraryPath(driver) : std::string());
    if (dlclose(driver) == 0)
    {
        void* p = system_dlopen(driverPath, (RTLD_NOLOAD));
        if (verboseMode)
        {
            //const std::string oldDriverFile(fs::path(oldDriverPath) / currentLibFileName);
            std::cout << "[DRIVER] Successfully unloaded driver library " << driver << std::endl
                      << "     lib " << std::quoted(driverPath) << std::endl
		      << "  exists " << std::boolalpha << (nullptr != p) << std::noboolalpha << std::endl;
        }
    }
    else if (verboseMode)
    {
        const char* error = dlerror();
        std::cout << "[DRIVER] Failed to unload driver library, error: "
		  << (error ? error : "<unknown-library>") << std::endl;
    }
}

static void freeDriver (const char* driver, bool verboseMode)
{
    freeDriver(system_dlopen(driver, RTLD_NOLOAD), verboseMode);
}

[[maybe_unused]] static void freeDriver (const std::string& driver, bool verboseMode)
{
    freeDriver(driver.c_str(), verboseMode);
}

extern "C" void* my_dlopen (const char* libFileName, int flags) __attribute__((alias("dlopen")));

void* dlopen (const char* libFileName, int flags)
{
    const std::string currentLibFileName(libFileName
                                    ? fs::path(libFileName).filename().string()
                                    : std::string());

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
    if ((__system_dlopen.load(std::memory_order_acquire) == null_dlopen) && (this_dlopen != sys_dlopen))
    {
        if (__verboseMode) std::cout << "[DRIVER] assign system_dlopen " << *(void**)&sys_dlopen << std::endl;
        __system_dlopen.compare_exchange_strong(null_dlopen, sys_dlopen,
			                        std::memory_order_acquire, std::memory_order_relaxed);
    }

    void* (*sys_dlsym)(int, const char*) = nullptr;
    *(void**)(&sys_dlsym) = dlsym(RTLD_DEFAULT, "dlsym");

    if ((__customVtfDriver.empty() == false) && compareNoCase(__vulkanDriverFileName, currentLibFileName))
    {
        if (__verboseMode)
        {
            std::cout << "[DRIVER] system_dlopen           " << *(void**)&__system_dlopen << std::endl;
            std::cout << "         system_dlsym            " << *(void**)&sys_dlsym << std::endl;
            std::cout << "         local_dlopen            " << *(void**)&this_dlopen << std::endl;
            std::cout << "         Handled loading driver  " << std::quoted(libFileName) << std::endl;
            std::cout << "         Trying to load driver   " << __customVtfDriver << std::endl;
        }

        void* oldDriver = system_dlopen(currentLibFileName, RTLD_NOLOAD);

        void* newDriver = __customDriver.load(std::memory_order_acquire);

	if (newDriver)
	{
            if (__verboseMode)
	    {
                std::cout << "[DRIVER] Library already loaded: " << newDriver
                          << ' ' << readLibraryPath(newDriver) << std::endl;
	    }
            return newDriver;
	}

        if (newDriver = system_dlopen(__customVtfDriver, RTLD_LAZY); newDriver)
        {
            if (verifyVulkanDriver(newDriver))
            {
                if (__verboseMode)
                {
                    std::cout << "[DRIVER] Successfully loaded driver " << readLibraryPath(newDriver) << std::endl;
                }

                freeDriver(oldDriver, __verboseMode);

                __customDriver.store(newDriver, std::memory_order_relaxed);

                if (__verboseMode)
                {
                    std::cout << "[DRIVER] Assign new driver handle: " << newDriver << std::endl;
                }

                return newDriver;
            }
            else if (__verboseMode)
            {
                std::cout << "[DRIVER] Given " << std::quoted(__customVtfDriver) << " is not valid Vulkan Driver\n"
                          << "         Instead trying to load " << std::quoted(libFileName) << std::endl;
            }
        }
        else if (__verboseMode)
        {
            std::cout << "[DRIVER] Driver library loading failed: " << readDlError() << std::endl
                      << "          Instead trying to load " << std::quoted(libFileName) << std::endl;
        }
    }

    return system_dlopen(libFileName, flags);
}
#endif // VULKAN_CUSTOM_DRIVER

DriverInitializer::DriverInitializer ()
{
}

void DriverInitializer::operator ()(const std::string& customVtfDriver, uint32_t verboseMode)
{
    static_cast<void>(customVtfDriver);

    if (verboseMode)
    {
        std::cout << "[DRIVER] DriverInitializer" << std::endl;
    }

#ifdef VULKAN_CUSTOM_DRIVER
    __verboseMode = verboseMode;

    dlopen_t this_dlopen, sys_dlopen;
    dlopen_t null_dlopen = nullptr;
    *(void**)(&sys_dlopen) = dlsym(RTLD_NEXT, "dlopen");
    *(void**)(&this_dlopen) = dlsym(RTLD_DEFAULT, "dlopen");
    if (this_dlopen != sys_dlopen)
    {
        __system_dlopen.compare_exchange_strong(null_dlopen, sys_dlopen,
			                        std::memory_order_acquire, std::memory_order_relaxed);
    }

    void* (*sys_dlsym)(int, const char*) = nullptr;
    *(void**)(&sys_dlsym) = dlsym(RTLD_DEFAULT, "dlsym");

    if (verboseMode)
    {
        std::cout << "[DRIVER] system_dlopen           " << *(void**)&__system_dlopen << std::endl;
        std::cout << "         system_dlsym            " << *(void**)&sys_dlsym << std::endl;
        std::cout << "         local_dlopen            " << *(void**)&this_dlopen << std::endl;
    }

    if (customVtfDriver.empty())
    {
	void* null_ptr = nullptr;

        void* vkDriver = system_dlopen(VULKAN_DRIVER, RTLD_NOLOAD);
	if (vkDriver)
        {
            __customDriver.compare_exchange_strong(null_ptr, vkDriver,
                           std::memory_order_acquire, std::memory_order_relaxed);
	    if (verboseMode)
	    {
                std::cout << "[DRIVER] Use builing driver : "
                          << vkDriver << ' ' << readLibraryPath(vkDriver) << std::endl;
	    }
	}
	else
	{
            std::cout << "[DRIVER] Trying to load driver   " << VULKAN_DRIVER << std::endl;
            vkDriver = system_dlopen(VULKAN_DRIVER, RTLD_LAZY);

            if (false == verifyVulkanDriver(vkDriver))
	    {
	    }

            __customDriver.compare_exchange_strong(null_ptr, vkDriver,
                           std::memory_order_acquire, std::memory_order_relaxed);
            if (verboseMode)
            {
                std::cout << "[DRIVER] Successfully loaded driver: "
                          << vkDriver << ' ' << readLibraryPath(vkDriver) << std::endl;
            }
        }
    }
    else
    {
	freeDriver(VULKAN_DRIVER, verboseMode);

        std::cout << "[DRIVER] Trying to load driver   " << customVtfDriver << std::endl;
        void* newDriver = system_dlopen(customVtfDriver, RTLD_LAZY);

        if (false == verifyVulkanDriver(newDriver))
	{
	}

	void* null_ptr = nullptr;
        __customDriver.compare_exchange_strong(null_ptr, newDriver,
			                        std::memory_order_acquire, std::memory_order_relaxed);
        if (verboseMode)
        {
            std::cout << "[DRIVER] Successfully loaded driver: " << newDriver << ' ' << readLibraryPath(newDriver) << std::endl;
        }

        __customVtfDriver = fs::absolute(customVtfDriver).string();
    }
#endif // VULKAN_CUSTOM_DRIVER
}

DriverInitializer::~DriverInitializer ()
{
#ifdef VULKAN_CUSTOM_DRIVER
    if (void* newDriver = __customDriver.load(std::memory_order_acquire); newDriver)
    {
        freeDriver(newDriver, getVtfVerboseMode());
	__customDriver.store(nullptr, std::memory_order_relaxed);
    }
#endif // VULKAN_CUSTOM_DRIVER
}

auto DriverInitializer::isCustomDriver () -> bool
{
#ifdef VULKAN_CUSTOM_DRIVER
    return (nullptr != __customDriver.load(std::memory_order_acquire));
#else
    return false;
#endif
}

static std::string readLibraryPath (void* lib)
{
    link_map* map = nullptr;

    if ((dlinfo(lib, RTLD_DI_LINKMAP, &map) != 0) || (nullptr == map))
    {
        throw std::runtime_error(dlerror());
    }

    return map->l_name;
}

auto DriverInitializer::getPlatformDriverFileName (bool& success) -> std::string
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
    const std::string path(readLibraryPath(dlopen(VULKAN_DRIVER, RTLD_NOLOAD | RTLD_LAZY)));
#endif
    success = (path.empty() == false);

    return fs::absolute(path);
}

auto getPlatformDriverProc (const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>
{
    void* addr = nullptr;
    if (handle)
    {
        icd = false;
        *(void**)(&addr) = dlsym(handle, procName);
        if (nullptr == addr)
        {
            void* (*icd_addr)(uint64_t, const char*) = nullptr;
            *(void**)(&icd_addr) = dlsym(handle, "vk_icdGetInstanceProcAddr");
            if (icd_addr)
            {
                icd = true;
                addr = (*icd_addr)(0, procName);
            }
        }
    }
    return addr;
}

auto DriverInitializer::getPlatformDriverProc (const char* procName) -> std::add_pointer_t<void>
{
    const int flags = RTLD_NOLOAD | RTLD_LAZY;

#ifdef VULKAN_CUSTOM_DRIVER
    void* handle = isCustomDriver()
                       ? __customDriver.load(std::memory_order_acquire)
                       : (__system_dlopen.load(std::memory_order_acquire)
                              ? system_dlopen(VULKAN_DRIVER, flags)
                              : dlopen(VULKAN_DRIVER, flags));
#else
    void* handle = dlopen(VULKAN_DRIVER, flags);
#endif

    bool icdAccess = false;
    void* addr = ::getPlatformDriverProc(procName, handle, icdAccess);

    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__
                  << "(handle=" << handle << ", name=" << std::quoted(procName) << ')';

	    std::cout << " = " << addr << (addr ? icdAccess ? " ICD" : " API" : "");
	    if (handle)
	    {
            std::cout << ", lib=" << readLibraryPath(handle);
	    }

	    std::cout << std::endl;
    }

    return addr;
}

