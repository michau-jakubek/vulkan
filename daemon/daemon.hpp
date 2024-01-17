#ifndef __DAEMON_HPP_INCLUDED__
#define __DAEMON_HPP_INCLUDED__

#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "shell.hpp"

#if 0
struct SharedData
{
	int shmKey;
	VkPhysicalDevice dev;
};

std::tuple<bool,SharedData,std::string> setSharedString (add_cref<SharedData>);
std::tuple<bool,SharedData,std::string> getSharedString (add_cref<SharedData>);
#endif // #if 0

SHARED_RESOURCE
std::tuple<ZDevice, std::string>
createSharedDevice (add_cptr<char> name, bool graphical);

SHARED_RESOURCE
std::shared_ptr<Shell> createOrGetSingletonShell (std::ostream&			output,
												  Shell::OnCommand		onCommand,
												  add_cref<std::string>	helpMessage = "This is help message",
												  add_cref<std::string>	historyFile = {});

#endif // __DAEMON_HPP_INCLUDED__
