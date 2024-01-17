#ifndef __INTERACTIVE_CONSOLE_HPP__
#define __INTERACTIVE_CONSOLE_HPP__

#include <fstream>
#include <sstream>
#if SYSTEM_OS_LINUX == 1
#include <termios.h>	// tc(g|s)etattr
#else
#include <Windows.h>
#endif

#if STANDALONE_PROJECT_CONSOLE
 template<class X> using add_ref = std::add_lvalue_reference_t<X>;
 template<class X> using add_cref = std::add_lvalue_reference_t<std::add_const_t<X>>;
#else
 #include "vtfZDeletable.hpp"
#endif

#ifndef UNREF
#define UNREF(x) static_cast<void>(x)
#endif

/***** EXAMPLE *****
void console(iconstream& con) {
    bool doContinue = true;
    while (doContinue) {
        int your_choice;
        std::cout <<
            "Enter you choice -\n"
            "1: Continue program\n"
            "2 or quit to exit application\n";
        con.read();
        con >> your_choice;
        auto status = con.check_input_and_reset();
        if (!status.first) {
            if (std::string("quit") == status.second) {
                std::cout << "Good bye\n";
                doContinue = false;
            }
            else std::cout << "Unknown option: " << status.second << std::endl;
        }
        else {
            switch (your_choice) {
                case 1:		std::cout << "You chosen to continue\n";						break;
                case 2:		std::cout << "Good bye\n"; doContinue = false;					break;
                default:	std::cout << "Unknown option: " << your_choice << std::endl;	break;
            }
        }
    }
}
int main() {
    const std::string hist("history.txt");
    iconstream con(std::cout, "> ");
    con.load(hist);
    console(con);
    con.save(hist);
    return 0;
}
*/

namespace icon {

class RawConsole;
class Console;
class iconstream;

union _uUser
{
	RawConsole*	pRawConsole;
	Console*	pConsole;
	iconstream*	pIconstream;
	_uUser (Console* p) : pConsole(p) {}
	_uUser (RawConsole* p) : pRawConsole(p) {}
	_uUser (iconstream* p) : pIconstream(p) {}
};

#if SYSTEM_OS_LINUX == 0
struct CmdStream
{
    HANDLE hConsole;
    CmdStream()
    {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        //BOOL status = GetConsoleMode(hConsole, &before);
    }
    CmdStream& operator<< (const std::string& s) {
        DWORD numberOfCharsWritten = 0;
        WriteConsole(hConsole, s.data(), (DWORD)s.length(), &numberOfCharsWritten, nullptr);
        return *this;
    }
    void flush() {}
    CmdStream& operator<< (char c) {
        DWORD numberOfCharsWritten = 0;
        WriteConsole(hConsole, &c, 1, &numberOfCharsWritten, nullptr);
        return *this;
    }
};
#endif

class RawConsole
{
#if SYSTEM_OS_LINUX == 1
    termios before, after;
#else
    DWORD before, after;
#endif
    size_t maxHistory;
    size_t tabw;
    std::string tabs;
    size_t pos_buff, pos_hist, pos_curr;
    std::vector<std::pair<size_t,std::vector<char>>> hist;
    std::reference_wrapper<std::vector<char>> buff;
    mutable std::ofstream m_debugPipe;  // watch -n 1 -d tail DEBUG
    bool processed;

    int readc(add_ref<int> count, add_ref<int> ctrl) const;
    void enableRaw();
    void disableRaw();
    void updateModified();
    std::vector<char> fromString(add_cref<std::string>) const;
    std::string stringFromCursor() const;
    void writeDebugPipe(int) const;
    void writeDebugPipe(const std::string&) const;
    void writeDebugPipe(const std::vector<char>&) const;
    static std::pair<size_t,size_t> getSize();

public:
    RawConsole(add_ref<std::ostream> str, size_t maxHistory = 100);
    ~RawConsole();

    inline size_t historyCount() const;
    size_t historyAdd(const std::string&);

	typedef void (* ON_NEW_LINE)(const RawConsole&, _uUser, const std::string&, bool&);
	void process(ON_NEW_LINE const& onNewLine, _uUser user);
    void listen(std::function<bool(int chr, int c_chr)>);

    size_t load(const std::string& fileName);
    bool save(const std::string& fileName) const;

    std::string prefix;
#if SYSTEM_OS_LINUX == 1
    std::ostream& stream;
#else
    CmdStream  cmdStream;
    CmdStream& stream;
#endif
    const std::pair<size_t,size_t> size;
};

std::istream& operator>>(std::istream& s, std::vector<std::string>& items);
std::istream& operator>>(std::istream& s, add_ref<std::pair<std::string, std::string>> items);

class Console : public RawConsole
{
public:
    Console(std::ostream& str, size_t maxHistory = 100) : RawConsole(str, maxHistory) { }

	void process(RawConsole::ON_NEW_LINE const& onNewLine, _uUser user)
    {
		RawConsole::process(onNewLine, user);
    }

    static std::pair<bool, std::string> check_input_and_reset(std::istream&);
};

class iconstream : public std::istringstream, public Console
{
	static void onNewLine(const RawConsole&, _uUser user, const std::string& line, bool& doContinue);
public:
    iconstream(std::ostream& ostream, const std::string& prompt, size_t maxHistory = 100);
    std::pair<bool, std::string> check_input_and_reset();
    void read();
};

} // namespace icon

#endif // __INTERACTIVE_CONSOLE_HPP__


