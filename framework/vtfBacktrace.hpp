#ifndef __VTF_BACKTRACE_HPP_INCLUDED__
#define __VTF_BACKTRACE_HPP_INCLUDED__

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

#endif // SYSTEM_OS_LINUX

#include "vtfVkUtils.hpp"

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

struct GlobalAppFlags
{
	vtf::Version	apiVer;
	vtf::Version	vulkanVer;
	vtf::Version	spirvVer;
	vtf::strings	layers;
	vtf::strings	suppressedVUIDs;
	uint32_t		physicalDeviceIndex;
	uint32_t		verbose;
	char			tmpDir[_MAX_PATH];
	std::string		assetsPath;
	std::string		thisAppPath;
	bool			spirvValidate;
	bool			genSpirvDisassembly;
	bool			nowerror;
	bool			debugPrintfEnabled;
	bool			noWarning_VUID_Undefined;

	GlobalAppFlags ();
};
const GlobalAppFlags& getGlobalAppFlags ();
void setGlobalAppFlags (const GlobalAppFlags&);

#endif // __VTF_BACKTRACE_HPP_INCLUDED__
