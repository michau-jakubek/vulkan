#include "vtfPlatformDriver.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
extern bool getVtfVerboseMode ();
extern bool compareNoCase (const std::string& a, const std::string& b);

using HANDLE = void*;
using SYMBOL = void*;

auto getPlatformDriverProc(const char* procName, HANDLE handle, bool& icd) -> SYMBOL;
bool verifyVulkanDriver (HANDLE h)
{
    bool icd = false;
    return !!getPlatformDriverProc("vkCreateInstance", h, icd);
}

DriverInitializer::DriverInitializer () { }
DriverInitializer::~DriverInitializer () { }

static HANDLE __customVkDriverHandle;
static std::string __customVkDriverName;

void DriverInitializer::operator ()(const std::string& customVkDriver, uint32_t verboseMode)
{
    __customVkDriverName = customVkDriver.empty() ? VULKAN_DRIVER : customVkDriver;
    __customVkDriverHandle = dlopen(__customVkDriverName.c_str(), RTLD_LAZY | RTLD_NOLOAD);

    if (verboseMode)
    {
        std::cout << "[DRIVER] DriverInitializer::(), module \"" << __customVkDriverName << "\" ";
        if (nullptr == __customVkDriverHandle)
            std::cout << "not ";
        std::cout << "loaded" << std::endl;
    }

    if (nullptr == __customVkDriverHandle)
    {
        __customVkDriverHandle = dlopen(__customVkDriverName.c_str(), RTLD_LAZY);
        if (verboseMode && __customVkDriverHandle)
            std::cout << "[DRIVER] DriverInitializer::(), loading module \"" << __customVkDriverName << "\"\n";
        else if (!__customVkDriverHandle && verboseMode)
            std::cerr << "[ERROR] dlopen: " << dlerror() << std::endl;
    }
}

auto DriverInitializer::getPlatformDriverFileName (bool& success) -> std::string
{
    if (__customVkDriverHandle == nullptr) {
        success = false;
        return "";
    }

    Dl_info info;
    SYMBOL symbol = dlsym(__customVkDriverHandle, "vkCreateInstance");
    if (symbol && dladdr(symbol, &info))
    {
        success = true;
        return std::string(info.dli_fname);
    }

    success = false;
    return "";
}

auto getPlatformDriverProc(const char* procName, HANDLE handle, bool& icd) -> SYMBOL
{
    SYMBOL proc = nullptr;
    if (handle)
    {
        icd = false;
        proc = dlsym(handle, procName);
        
        if (nullptr == proc)
        {
            void* (*icd_proc)(uint64_t, const char*) = nullptr;
            icd_proc = (decltype(icd_proc))dlsym(handle, "vk_icdGetInstanceProcAddr");
            
            if (icd_proc)
            {
                icd = true;
                proc = (*icd_proc)(0, procName);
            }
        }
    }
    return proc;
}

auto DriverInitializer::getPlatformDriverProc (const char* procName) -> SYMBOL
{
    HANDLE handle = dlopen(__customVkDriverName.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    
    if (handle != __customVkDriverHandle) {
        std::ostringstream os;
        os << "Different handles \"" << __customVkDriverName << "\"=" << handle
           << " and " << __customVkDriverHandle;
        throw std::runtime_error(os.str()); 
    }

    bool icdAccess = false;
    SYMBOL proc = ::getPlatformDriverProc(procName, handle, icdAccess);

    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__ 
                  << "(handle=" << handle << ", name=" << std::quoted(procName) << ')';
        std::cout << " = " << std::hex << proc << std::dec << (proc ? (icdAccess ? " ICD" : " API") : "");
        
        if (handle)
        {
            Dl_info info;
            if (proc && dladdr(proc, &info))
                std::cout << ", lib=" << info.dli_fname;
        }
        std::cout << std::endl;
    }

    return proc;
}

