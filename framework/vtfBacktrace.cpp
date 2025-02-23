#include "vtfBacktrace.hpp"

#if SYSTEM_OS_LINUX
 #include <cstdlib>
 #include <string_view>
 #include <dlfcn.h>
 #include <link.h>
#elif SYSTEM_OS_WINDOWS
 #include <windows.h>
 #include <dbghelp.h>
#endif

#include "demangle.hpp"

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) std::extent<decltype(a)>::value
#endif // ARRAY_LENGTH

#if SYSTEM_OS_LINUX
static int addr2line(const char* bin, const char* addr, char* output, size_t size)
{		
	char cmd[1024];
	char path[256];
	const auto rpl = realpath(bin, path); UNREF(rpl);
	snprintf(cmd, ARRAY_LENGTH(cmd), "addr2line -Cfe %s %s", path, addr);
	FILE* pipe = popen(cmd, "r");
	if (!pipe) return -1;
	int readed = (int)fread(output, 1, size-1, pipe);
	pclose(pipe);
	output[readed] = '\0';
	for (int i = 0; i <= readed; ++i)
    { 
		if (output[i] == '\n')
            output[i] = ' ';	
	}
	return 0;
}

static bool getaddr(const char* input, char* addr, size_t len)
{
    std::string_view s(input);
    auto right = s.find(']');
    auto left = s.find('[');
    if (left == s.npos || right == s.npos) return false;
    size_t i = 0;
    left += 1;
    for (i = 1; i < len && left < right; ++left, ++i)
    {
        addr[i-1] = input[left];
    }
    addr[i-1] = '\0';
    return true;
}

static long long int addroffset(const char* saddr, const void* base)
{
    long long int addr = 0;
	sscanf(saddr, "%lli", &addr);
    return (addr - reinterpret_cast<long long int>(base));
}

static bool getaddr(const char* input, const void* base, char* addr, size_t len)
{
    char tmp[32];
    if (getaddr(input, tmp, ARRAY_LENGTH(tmp)))
    {
        const long long int diff = addroffset(tmp, base);
        snprintf(addr, len, "%llx", diff);
        return true;
    }
	return false;
}

std::size_t printBts (std::ostream& ss, void** bts, std::size_t nbt, std::size_t skip)
{
    char        addr[32];
    char        name[1024];
	uint32_t	ibt = 0;

	/* alternative: backtrace_symbols_fd(traces, nbt, STDOUT_FILENO) */

	char** symbols = backtrace_symbols(bts, (int)nbt);
	if (symbols == NULL) return 0;

	for (ibt = (uint32_t)skip; ibt < nbt; ++ibt)
	{
		Dl_info dlinfo; // link this with -ldl
		if (!dladdr(bts[ibt], &dlinfo)) break;

		ss << ">---" << std::endl;
        if (getaddr(symbols[ibt], dlinfo.dli_fbase, addr, ARRAY_LENGTH(addr))
                && addr2line(dlinfo.dli_fname, addr, name, ARRAY_LENGTH(name)) == 0)
        {
            ss << name << std::endl;
        }
		else
		{
            ss << symbols[ibt] << std::endl;
		}
	}

	free(symbols);

	return (skip < nbt) ? (nbt - skip) : 0;
}

#elif SYSTEM_OS_WINDOWS

void PrintStackTrace (std::ostream& str, const int maxFrames, const int)
{
    std::vector<void*> stack(maxFrames);
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE process = GetCurrentProcess();

    SymInitialize(process, NULL, TRUE);
    frames = CaptureStackBackTrace(0, maxFrames, stack.data(), NULL);

    symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (unsigned int i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        str << frames - i - 1 << ": " << symbol->Name << " - 0x" << symbol->Address << std::endl;
		if (strcmp(symbol->Name, "main") == 0)
			break;
    }

    free(symbol);
}

#endif // SYSTEM_OS_LINUX | SYSTEM_OS_WINDOWS

static GlobalAppFlags globalAppFlags;
const GlobalAppFlags& getGlobalAppFlags () { return globalAppFlags; }
void setGlobalAppFlags(const GlobalAppFlags& flags) { globalAppFlags = flags; }
GlobalAppFlags::GlobalAppFlags()
	: vtfVer					(1, 0)
	, apiVer					(1, 1)
	, vulkanVer					(1, 0)
	, spirvVer					(1, 0)
	, layers					()
	, excludedDevExtensions		()
	, suppressedVUIDs			()
	, physicalDeviceIndex		(0)
	, verbose					(0)
	, compilerIndex				(0)
	, tmpDir					()
	, assetsPath				()
	, thisAppPath				()
	, vulkanDriver				()
	, spirvValidate				(false)
	, genSpirvDisassembly		(false)
	, nowerror					(false)
    , debugPrintfEnabled        (false)
    , noWarning_VUID_Undefined  (false)
{
}

int qqq;
