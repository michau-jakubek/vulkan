#include "vtfProgressRecorder.hpp"
#include "vtfZDeletable.hpp"

namespace vtf
{

ProgressRecorder::ProgressRecorder ()
	: m_start(std::chrono::high_resolution_clock::now())
{
}

void ProgressRecorder::stamp (const std::string& text, bool label)
{
	m_entries.emplace_back(Entry{ std::chrono::high_resolution_clock::now(), text, label });
}

void ProgressRecorder::print(std::ostream& stream, bool newLineAtEnd) const
{
	auto start = m_start;
	stream << "Start application:";
	for (add_cref<Entry> e : m_entries)
	{
		const auto between = std::chrono::duration_cast<std::chrono::milliseconds>(e.when - start).count();
		const auto fromstart = std::chrono::duration_cast<std::chrono::milliseconds>(e.when - m_start).count();
		stream << std::endl << e.text << " - duration: " << between << " ms, total: " << fromstart << " ms";
		start = e.when;
	}
	if (newLineAtEnd)
	{
		stream << std::endl;
	}
}

} // namespace vtf
