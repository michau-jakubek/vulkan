#ifndef __VTF_BACKTRACE_HPP_INCLUDED__
#define __VTF_BACKTRACE_HPP_INCLUDED__

#if SYSTEM_OS_LINUX == 1

#include <execinfo.h>

std::size_t printBts (std::ostream& ss, void** bts, std::size_t nbt, std::size_t skip = 0);

template<size_t Depth>
void printBacktrace (std::ostream& ss, std::size_t skip = 0)
{
	void* bts[Depth];
	const int	nbt = backtrace(bts, Depth);
	printBts(ss, bts, nbt, skip);
}

#endif // SYSTEM_OS_LINUX

bool getAppVerboseFlag ();
void setAppVerboseFlag (bool);

#endif // __VTF_BACKTRACE_HPP_INCLUDED__
