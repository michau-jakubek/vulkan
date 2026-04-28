#include "vtfPlatformDriver.hpp"

#include <windows.h>

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
    bool icd = false;
    return !!getPlatformDriverProc("vkCreateInstance", h, icd);
}

DriverInitializer::DriverInitializer () { }
DriverInitializer::~DriverInitializer () { }

static HMODULE __customVkDriverHandle;
static std::string __customVkDriverName;

void DriverInitializer::operator ()(const std::string& customVkDriver, uint32_t verboseMode)
{
    __customVkDriverName = customVkDriver.empty() ? VULKAN_DRIVER : customVkDriver.c_str();
    __customVkDriverHandle = GetModuleHandleA(__customVkDriverName.c_str());

    if (verboseMode)
    {
        std::cout << "[DRIVER] DriverInitializer::(), module \"" << __customVkDriverName << "\" ";
        if (nullptr == __customVkDriverHandle)
            std::cout << "not ";
        std::cout << "loaded" << std::endl;
    }

    if (nullptr == __customVkDriverHandle)
    {
        __customVkDriverHandle = LoadLibraryA(__customVkDriverName.c_str());
        if (verboseMode)
            std::cout << "[DRIVER] DriverInitializer::(), loading module \"" << __customVkDriverName << "\"\n";
    }
}


auto DriverInitializer::getPlatformDriverFileName (bool& success) -> std::string
{
    std::string name;
    std::vector<TCHAR> p(1024);
    const HMODULE handle = GetModuleHandle(fs::path(VULKAN_DRIVER).filename().string().c_str());

    if (success = (handle != nullptr); success)
    {
        GetModuleFileName(handle, p.data(), (DWORD)p.size());
        name.assign(p.data());
    }

    return name;
}

auto getPlatformDriverProc(const char* procName, void* handle, bool& icd) -> std::add_pointer_t<void>
{
    void* proc = nullptr;
    if (handle)
    {
        icd = false;
        proc = GetProcAddress((HMODULE)handle, procName);
        if (nullptr == proc)
        {
            FARPROC(*icd_proc)(uint64_t, const char*) = nullptr;
            icd_proc = (decltype(icd_proc))GetProcAddress((HMODULE)handle, "vk_icdGetInstanceProcAddr");
            if (icd_proc)
            {
                icd = true;
                proc = (*icd_proc)(0, procName);
            }
        }
    }
    return proc;
}

auto DriverInitializer::getPlatformDriverProc (const char* procName) -> std::add_pointer_t<void>
{
    const HMODULE handle = GetModuleHandle(fs::path(__customVkDriverName).filename().string().c_str());
    if (handle != __customVkDriverHandle) {
        std::ostringstream os;
        os << "Different handles \"" << __customVkDriverName << "\"=" << handle
            << " and " << __customVkDriverHandle;
        throw std::runtime_error(os.str());
    }

    bool icdAccess = false;
    FARPROC proc = (FARPROC)::getPlatformDriverProc(procName, handle, icdAccess);

    if (getVtfVerboseMode())
    {
        std::cout << "[DRIVER] " << __func__
            << "(handle=" << handle << ", name=" << std::quoted(procName) << ')';
        std::cout << " = " << std::hex << proc << std::dec << (proc ? icdAccess ? " ICD" : " API" : "");
        if (handle)
        {
            std::vector<TCHAR> p(1024);
            GetModuleFileName(handle, p.data(), (DWORD)p.size());

            std::cout << ", lib=" << p.data();
        }
        std::cout << std::endl;
    }

    return proc;
}

