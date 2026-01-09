#ifndef __VTF_APP_HPP_INCLUDED__
#define __VTF_APP_HPP_INCLUDED__

#include <cstdint>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#define VTF_API_CALL __cdecl
#else
#define VTF_API_CALL
#endif


typedef void(VTF_API_CALL* pVTF_API_GetVersion)(uint32_t* major, uint32_t* minor, uint32_t* patch, uint32_t* rev);
typedef int(VTF_API_CALL* pVTF_API_RunTest)(const char* multiStringCommandLine);

struct VTF_API
{
	pVTF_API_GetVersion GetVersion;
	pVTF_API_RunTest	RunTest;
};
typedef VTF_API *VTF_API_PTR;

#define VTF_GetAPI_PROC_NAME "VTF_GetAPI"
// Find symbol VTF_GetApi then call it
typedef void(VTF_API_CALL* pVTF_GetAPI)(
	void* physicalDevice,
	void* instance, uint32_t instanceID,
	void* logicalDevice, uint32_t deviceID,
	char* const* requiredInstanceExtensions,
	uint32_t requiredInstanceExtensionCount,
	uint32_t graphicsQueueFamilyIndex,
	uint32_t computeQueueFamilyIndex,
	VTF_API_PTR* ppVTF_API);

#endif // __VTF_APP_HPP_INCLUDED__
