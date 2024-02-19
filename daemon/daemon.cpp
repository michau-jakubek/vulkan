#include "vtfCUtils.hpp"
#include "vtfBacktrace.hpp"
#include "vtfZUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfCanvas.hpp"

#include "vtfZSharedDevice.hpp"

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


static std::vector<vtf::SharedDevice> privateSharedDeviceList;
static std::vector<std::shared_ptr<Shell>> privateSingletonShell;

vtf::SharedDevice createOrGetSharedDevice (add_cptr<char> name, bool graphical)
{
	if (privateSharedDeviceList.empty())
	{
		privateSharedDeviceList.emplace_back(name, graphical);
	}
	return privateSharedDeviceList.at(0);
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
