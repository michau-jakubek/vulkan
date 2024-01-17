#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfCanvas.hpp"

#include "daemon.hpp"

#if SYSTEM_OS_LINUX
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <system_error>
#include <sstream>
#endif

#include <memory>

namespace vtf
{
struct SharedDevice : GlfwInitializerFinalizer
{
	add_cref<GlobalAppFlags>	m_globalAppFlags;
	VkAllocationCallbacksPtr	m_callbacks;
	VkDebugUtilsMessengerEXT	m_debugMessenger;
	VkDebugReportCallbackEXT	m_debugReport;
	const bool					m_graphical;
	ZDevice						m_device;
	SharedDevice (add_cptr<char> name, bool graphical);
	virtual ~SharedDevice ();
};

extern strings			getGlfwRequiredInstanceExtensions ();
extern ZGLFWwindowPtr	createWindow (const CanvasStyle& style, const char* title, add_ptr<void> windowUserPointer);
extern ZSurfaceKHR		createSurface (ZInstance instance, VkAllocationCallbacksPtr callbacks, ZGLFWwindowPtr window);

SharedDevice::SharedDevice (add_cptr<char> name, bool graphical)
	: GlfwInitializerFinalizer(graphical)
	, m_globalAppFlags	(getGlobalAppFlags())
	, m_callbacks		(getAllocationCallbacks())
	, m_debugMessenger	()
	, m_debugReport		()
	, m_graphical		(graphical)
{
	ZInstance instance = createInstance(name, m_callbacks,
									m_globalAppFlags.layers, {/*instanceExtensions*/},
									&m_debugMessenger, this, &m_debugReport, this,
									m_globalAppFlags.apiVer, m_globalAppFlags.debugPrintfEnabled);
	ZPhysicalDevice physDev = selectPhysicalDevice(make_signed(m_globalAppFlags.physicalDeviceIndex), instance, {/*deviceExtensions*/});
	VkPhysicalDeviceFeatures2 resultFeatures = makeVkStruct();
	auto onEnablingFeatures = [&](ZPhysicalDevice physicalDevice, add_ref<strings> extensions)
	{
		UNREF(extensions);
		vkGetPhysicalDeviceFeatures2(*physicalDevice, &resultFeatures);
		return resultFeatures;
	};

	ZGLFWwindowPtr	window;
	ZSurfaceKHR		surface;
	if (graphical)
	{
		window = createWindow(Canvas::DefaultStyle, name, this);
		surface = createSurface(instance, m_callbacks, window);
	}
	m_device = createLogicalDevice(physDev, onEnablingFeatures, surface, m_globalAppFlags.debugPrintfEnabled);
}

SharedDevice::~SharedDevice ()
{
	ZInstance i = m_device.getParam<ZPhysicalDevice>().getParam<ZInstance>();
	destroyDebugMessenger(i, m_callbacks, m_debugMessenger);
	destroyDebugReport(i, m_callbacks, m_debugReport);
}

} // namespace vtf

static std::vector<vtf::SharedDevice> privateSharedDeviceList;
static std::vector<std::shared_ptr<Shell>> privateSingletonShell;
namespace vtf
{
	SHARED_RESOURCE ZDevice globalSharedDevice; 
}

std::tuple<ZDevice, std::string>
createSharedDevice (add_cptr<char> name, bool graphical)
{
	if (privateSharedDeviceList.empty())
	{
		privateSharedDeviceList.emplace_back(name, graphical);
		vtf::globalSharedDevice = privateSharedDeviceList.at(0).m_device;
	}
	return { privateSharedDeviceList.at(0).m_device, {} };
}

std::shared_ptr<Shell> createOrGetSingletonShell (std::ostream&			output,
												  Shell::OnCommand		onCommand,
												  add_cref<std::string>	helpMessage,
												  add_cref<std::string>	historyFile)
{
	if (privateSingletonShell.empty())
	{
		privateSingletonShell.emplace_back(
					std::make_shared<Shell>(output, onCommand, helpMessage, historyFile));
	}
	return privateSingletonShell.at(0);
}

#if 0
auto streamAppend = [](auto& os, const auto&... x) -> auto&
{
	SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << x)...));
	os.flush();
	return os;
};

std::tuple<bool,SharedData,std::string> getSharedString (add_cref<SharedData> sdIn)
{
	// rouded up to PAGE_SIZE
	const char nl = '\n';
	std::ostringstream os;
	SharedData sdTmp(sdIn);
	const key_t key = sdIn.shmKey;
	streamAppend(os, __func__, "received key = ", key, nl);

	void* shm = shmat(key, 0, 0);
	std::error_code e_shmat(errno, std::generic_category());
	streamAppend(os, "shmat returned addr = ", shm, nl);
	if (shm == (void*)(-1))
	{
		streamAppend(os, "shmat", "failed", e_shmat.message(), nl);
		return { false, sdTmp, os.str() };
	}

	add_ref<SharedData> sdOut = *((SharedData*)shm);

	return { true, sdOut, os.str() };
}

std::tuple<bool,SharedData,std::string> setSharedString (add_cref<SharedData> sdIn)
{
	const char nl = '\n';
	std::ostringstream os;
	streamAppend(os, nl);

	SharedData sdTmp {};

	int id = shmget(IPC_PRIVATE, sizeof(SharedData), SHM_R|SHM_W);
	std::error_code e_shmget(errno, std::generic_category());
	streamAppend(os, "shmget returned id = ", id, nl);
	sdTmp.shmKey = id;
	if (id < 0)
	{
		streamAppend(os, "shmget failed", e_shmget.message(), nl);
		return { false, sdTmp, os.str() };
	}

	void* shm = shmat(id, 0, 0);
	std::error_code e_shmat(errno, std::generic_category());
	streamAppend(os, "shmat returned addr = ", shm, nl);
	if (shm == (void*)(-1))
	{
		streamAppend(os, "shmat", "failed", e_shmat.message(), nl);
		return { false, sdTmp, os.str() };
	}

	/*
	int res = shmdt(shm);
	std::error_code e_shmdt(errno, std::generic_category());
	streamAppend(os, "shmdt returned res = ", res, nl);
	if (res < 0)
	{
		streamAppend(os, "shmdt failed", e_shmdt.message(), nl);
		return { false, id, shm, os.str() };
	}
	*/

	int res = shmctl(id, IPC_RMID, 0);
	std::error_code e_shmctl_rmid(errno, std::generic_category());
	streamAppend(os, "shmctl(RMID) returned res = ", res, nl);
	if (res < 0)
	{
		streamAppend(os, "shmctl(RMOID) failed", e_shmctl_rmid.message(), nl);
		return { false, sdTmp, os.str() };
	}

	shmid_ds ds;
	res = shmctl(id, IPC_INFO, &ds);
	std::error_code e_shmctl_info(errno, std::generic_category());
	streamAppend(os, "shmctl(INFO) returned res = ", res, nl);
	streamAppend(os, "Last time: ", std::ctime(&ds.shm_atime), nl);
	if (res < 0)
	{
		streamAppend(os, "shmctl(INFO) failed", e_shmctl_info.message(), nl);
		return { false, sdTmp, os.str() };
	}

	add_ref<SharedData> sdOut = *((SharedData*)shm);
	sdOut.dev = sdIn.dev;
	sdOut.shmKey = id;

	return { true, sdOut, os.str() };
}
#endif // #if 0
