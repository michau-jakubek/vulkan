#ifndef __VTF_BACKTRACE_HPP_INCLUDED__
#define __VTF_BACKTRACE_HPP_INCLUDED__

#include <cstdint>
#include <cstdlib>
#include <ostream>

#if SYSTEM_OS_LINUX == 1

#include <execinfo.h>

std::size_t printBts (std::ostream& ss, void** bts, std::size_t nbt, std::size_t skip = 0);

template<size_t Depth>
void printBacktrace (std::ostream& ss, std::size_t skip = 0)
{
	void* bts[Depth];
	const uint32_t	nbt = static_cast<uint32_t>(backtrace(bts, Depth));
	printBts(ss, bts, nbt, skip);
}

#elif SYSTEM_OS_WINDOWS

void PrintStackTrace (std::ostream& str, const int maxFrames, const int skip = 0);

#endif // SYSTEM_OS_LINUX | SYSTEM_OS_WINDOWS

#include "vtfVkUtils.hpp"

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

struct GlobalAppFlags
{
	vtf::Version	vtfVer; // copy of main.hpp:CurrentVtfVersion
	vtf::Version	apiVer;
	vtf::Version	vulkanVer;
	vtf::Version	spirvVer;
	vtf::strings	layers;
	vtf::strings	deviceExtensions;
	vtf::strings	excludedDevExtensions;
	vtf::strings	suppressedVUIDs;
	uint32_t		physicalDeviceIndex;
	uint32_t		vtfAsDllInstance;
	uint32_t		verbose;
	uint32_t		compilerIndex;
	char			tmpDir[_MAX_PATH];
	std::string		cmdSignature; // filled in main.cpp::parseParams
	std::string		assetsPath;
	std::string		thisAppPath;
	std::string		vulkanDriver;
	std::string		spirvValArgs;
	bool			spirvValidate;
	bool			genSpirvDisassembly;
	bool			nowerror;
	bool			debugPrintfEnabled;
	bool			noWarning_VUID_Undefined;

	GlobalAppFlags ();
};
const GlobalAppFlags& getGlobalAppFlags ();
void setGlobalAppFlags (const GlobalAppFlags&);

extern int qqq;

#endif // __VTF_BACKTRACE_HPP_INCLUDED__
