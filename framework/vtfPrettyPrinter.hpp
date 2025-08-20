#ifndef __VTF_PRETTY_PRINTER_HPP_INCLUDED__
#define __VTF_PRETTY_PRINTER_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include "vtfCUtils.hpp"
#include <sstream>
#include <vector>

namespace vtf
{

class PrettyPrinter
{
	typedef add_ref<std::ostream> Cursor_;
	std::vector<std::ostringstream> m_cursors;
	void printFrames(
		Cursor_ str,
		add_cref<std::vector<uint32_t>> sequence,
		add_cref<std::vector<strings>> frames,
		uint32_t maxlinelength, uint32_t space) const;

public:
	using Cursor = Cursor_;
	Cursor getCursor (uint32_t cursor);
	Cursor merge (add_cref<std::vector<uint32_t>> cursorMask,
					Cursor str, uint32_t space = 4u) const;
	Cursor selfTest (Cursor str) const;
};

}

#endif // __VTF_PRETTY_PRINTER_HPP_INCLUDED__
