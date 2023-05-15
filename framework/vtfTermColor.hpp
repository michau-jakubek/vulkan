#ifndef VTF_TERMCOLOR_HPP_INCLUDED
#define VTF_TERMCOLOR_HPP_INCLUDED

#include "vtfZDeletable.hpp"
#include <iostream>

/*
CEND      = '\33[0m'
CBOLD     = '\33[1m'
CITALIC   = '\33[3m'
CURL      = '\33[4m'
CBLINK    = '\33[5m'
CBLINK2   = '\33[6m'
CSELECTED = '\33[7m'

CBLACK  = '\33[30m'
CRED    = '\33[31m'
CGREEN  = '\33[32m'
CYELLOW = '\33[33m'
CBLUE   = '\33[34m'
CVIOLET = '\33[35m'
CBEIGE  = '\33[36m'
CWHITE  = '\33[37m'

CBLACKBG  = '\33[40m'
CREDBG    = '\33[41m'
CGREENBG  = '\33[42m'
CYELLOWBG = '\33[43m'
CBLUEBG   = '\33[44m'
CVIOLETBG = '\33[45m'
CBEIGEBG  = '\33[46m'
CWHITEBG  = '\33[47m'

CGREY    = '\33[90m'
CRED2    = '\33[91m'
CGREEN2  = '\33[92m'
CYELLOW2 = '\33[93m'
CBLUE2   = '\33[94m'
CVIOLET2 = '\33[95m'
CBEIGE2  = '\33[96m'
CWHITE2  = '\33[97m'
*/

namespace vtf
{

struct TermColor
{
	enum Color {
		BLACK = 0, DBLUE, DGREEN, DCYAN, DRED, DMAGENTA, DYELLOW, DWHITE,
		     GRAY,  BLUE,  GREEN,  CYAN,  RED,  MAGENTA,  YELLOW,  WHITE, 
		END, COLOR_MAX_ENUM = END
	} color_;
	TermColor(Color color = END) : color_(color) {}
	static void selfTest(add_ref<std::ostream> str);
};
inline TermColor color(TermColor::Color color = TermColor::END) {
	return TermColor(color);
}

add_ref<std::ostream> operator<<(add_ref<std::ostream> str, add_cref<TermColor> color);

} // namespace vtf

#endif // VTF_TERMCOLOR_HPP_INCLUDED
