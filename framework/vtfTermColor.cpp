#include "vtfVkUtils.hpp"
#include "vtfTermColor.hpp"

#if SYSTEM_OS_WINDOWS

#include <Windows.h>

HANDLE getWinConsoleTextHandle(std::ostream& stream)
{
    if (&stream == &std::cout)
        return GetStdHandle(STD_OUTPUT_HANDLE);
    else if (&stream == &std::cerr || &stream == &std::clog)
        return GetStdHandle(STD_ERROR_HANDLE);
    return nullptr;
}

WORD defaultAttributes;
HANDLE hConsole = INVALID_HANDLE_VALUE;

void changeWinConsoleTextColorAttributes(std::ostream& stream, int foreground = (-1), int background = (-1))
{
    if (INVALID_HANDLE_VALUE == hConsole)
    {
        hConsole = getWinConsoleTextHandle(stream);
        if (INVALID_HANDLE_VALUE == hConsole)
            return;
    }

    if (0 == defaultAttributes)
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(hConsole, &info))
            return;
        defaultAttributes = info.wAttributes;
    }

    if (foreground == -1 && background == -1)
    {
        SetConsoleTextAttribute(hConsole, defaultAttributes);
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(hConsole, &info))
        return;

    if (foreground != -1)
    {
        info.wAttributes &= ~(info.wAttributes & 0x0F);
        info.wAttributes |= static_cast<WORD>(foreground);
    }

    if (background != -1)
    {
        info.wAttributes &= ~(info.wAttributes & 0xF0);
        info.wAttributes |= static_cast<WORD>(background);
    }

    SetConsoleTextAttribute(hConsole, info.wAttributes);
}

#endif // SYSTEM_OS_WINDOWS

namespace vtf
{

add_ref<std::ostream> operator<<(add_ref<std::ostream> str, add_cref<TermColor> color)
{
#if SYSTEM_OS_WINDOWS
	if (color.color_ == TermColor::END)
        ::changeWinConsoleTextColorAttributes(str);
	else ::changeWinConsoleTextColorAttributes(str, color.color_);
#else
	UNREF(color);
#endif
	return str;
}

void TermColor::selfTest(add_ref<std::ostream> str)
{
	str << color(BLUE)		<< "BLUE"		<< color() << std::endl;
	str << color(DBLUE)		<< "DBLUE"		<< color() << std::endl;
	str << color(GREEN)		<< "GREEN"		<< color() << std::endl;
	str << color(DGREEN)	<< "DGREEN"		<< color() << std::endl;
	str << color(CYAN)		<< "CYAN"		<< color() << std::endl;
	str << color(DCYAN)		<< "DCYAN"		<< color() << std::endl;
	str << color(MAGENTA) 	<< "MAGEBTA"	<< color() << std::endl;
	str << color(DMAGENTA)	<< "DMAGENTA"	<< color() << std::endl;
	str << color(RED)		<< "RED"		<< color() << std::endl;
	str << color(DRED)		<< "DRED"		<< color() << std::endl;
	str << color(YELLOW)	<< "YELLOW"		<< color() << std::endl;
	str << color(DYELLOW)	<< "DYELLOW"	<< color() << std::endl;
	str << color(WHITE)		<< "WHITE"		<< color() << std::endl;
	str << color(DWHITE)	<< "DWHITE"		<< color() << std::endl;
	str << color(GRAY)		<< "GRAY"		<< color() << std::endl;
}

} // namespace vtf
