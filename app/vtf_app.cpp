#include "demangle.hpp"
#include "vtfZUtils.hpp"
#include "vtfCommandLine.hpp"
#include "allTests.hpp"
#include "vtf_app.hpp"
#include "main.hpp"

#include <unordered_map>
#include <optional>

#if defined(_WIN32) || defined(_WIN64)
#define VTF_API_EXPORT __declspec(dllexport)
#else
#define VTF_API_EXPORT
#endif

int parseParams(
	add_ref<vtf::CommandLine> cmd,
	add_ref<TestRecord> testRecord,
	add_ref<std::string> testName,
	add_ref<bool> performTest);

struct VTF_API_EXT : vtf::VtfAppSharedObject, VTF_API
{
};

void VTF_API_CALL VTF_API_GetVersion (uint32_t* major, uint32_t* minor, uint32_t* patch, uint32_t* rev)
{
	*major = CurrentVtfVersion.nmajor;
	*minor = CurrentVtfVersion.nminor;
	*patch = CurrentVtfVersion.npatch;
	*rev = CurrentVtfVersion.nvariant;
}

int VTF_API_CALL VTF_API_RunTest (add_cptr<char> multiStringCommandLine)
{
	bool performTest;
	std::string testName;
	TestRecord testRecord;
	vtf::TriLogicInt testResult;
	vtf::CommandLine cmd(std::string(multiStringCommandLine,
									*vtf::multiStringLength(multiStringCommandLine, 1024)),
									"VTF_AS_DLL");
	const int res = parseParams(cmd, testRecord, testName, performTest);
	if (res == 0 && performTest)
	{
		try
		{
			testResult = (*testRecord.call)(testRecord, cmd);
		}
		catch (std::exception& e)
		{
			std::cout << e.what() << std::endl;
			testResult = (-1);
		}
	}
	return testResult.hasValue() ? testResult.value() : (-1);
}

extern "C" VTF_API_EXPORT void VTF_API_CALL VTF_GetAPI (
	void* phys,
	void* instance, uint32_t instanceID,
	void* device, uint32_t deviceID,
	char* const* requiredInstanceExtensions,
	uint32_t requiredInstanceExtensionCount,
	uint32_t graphicsQueueFamilyIndex,
	uint32_t computeQueueFamilyIndex,
	VTF_API_PTR* ppVTF_API)
{
	ASSERTMSG(instance && device && ppVTF_API, "Params: instance, device and ppVTF_API must not be null");
	VTF_API_EXT api;
	api.GetVersion	= VTF_API_GetVersion;
	api.RunTest		= VTF_API_RunTest;
	api.phys		= reinterpret_cast<VkPhysicalDevice>(phys);
	api.instance	= reinterpret_cast<VkInstance>(instance);
	api.device		= reinterpret_cast<VkDevice>(device);
	api.instanceID	= instanceID;
	api.deviceID	= deviceID;
	api.graphicsQueueFamilyIndex	= graphicsQueueFamilyIndex;
	api.computeQueueFamilyIndex		= computeQueueFamilyIndex;
	
	const std::pair<uint32_t, uint32_t> key({ instanceID, deviceID });
	auto p = std::make_shared<VTF_API_EXT>(api);

	p->requiredInstanceExtensions.resize(requiredInstanceExtensionCount);
	for (uint32_t i = 0u; i < requiredInstanceExtensionCount; ++i)
		p->requiredInstanceExtensions[i] = requiredInstanceExtensions[i];

	vtf::VtfAppSharedObject::get()[key] = p;
	*ppVTF_API = std::dynamic_pointer_cast<VTF_API>(p).get();
}
