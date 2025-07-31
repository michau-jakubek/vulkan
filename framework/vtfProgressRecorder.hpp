#ifndef __VTF_PROGRESS_RECORDER_HPP_INCLUDED__
#define __VTF_PROGRESS_RECORDER_HPP_INCLUDED__

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace vtf
{

struct ProgressRecorder
{
	using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;

	struct Entry
	{
		time_point when;
		std::string text;
		bool label;
	};

	ProgressRecorder ();
	void stamp (const std::string& text, bool label = false);
	void print (std::ostream& stream, bool newLineAtEnd = true) const;

private:
	const time_point	m_start;
	std::vector<Entry>	m_entries;
};

} // namespace vtf

#endif // __VTF_PROGRESS_RECORDER_HPP_INCLUDED__
