#include "vtfPrettyPrinter.hpp"
#include "vtfVkUtils.hpp"
#include "vtfFormatUtils.hpp"
#include <numeric>

namespace vtf
{

void PrettyPrinter::printFrames (
	Cursor out,
	add_cref<std::vector<uint32_t>> sequence,
	add_cref<std::vector<strings>> frames,
	add_cref<std::vector<uint32_t>> maxLineLengthPerFrames,
	uint32_t maxLineLength, uint32_t space,
	bool fitIndividualColumn) const
{
	uint32_t maxFrameRowCount = 0u;
	for (add_cref<strings> frame : frames)
		maxFrameRowCount = std::max(data_count(frame), maxFrameRowCount);

	std::vector<uint32_t> lastFrameLinesLength(maxFrameRowCount);

	for (uint32_t row = 0u; row < maxFrameRowCount; ++row)
	{
		for (uint32_t frameIdx = 0u; frameIdx < data_count(sequence); ++frameIdx)
		{
			add_cref<strings> frame = frames[sequence[frameIdx]];
			add_cref<std::string> text = frame[row];
			const auto rowLen = frameIdx
				? fitIndividualColumn
					? maxLineLengthPerFrames[sequence[frameIdx - 1u]]
					: maxLineLength
				: maxLineLength;

			const auto padLen = (rowLen > lastFrameLinesLength[row]) ? (rowLen - lastFrameLinesLength[row]) : 0u;
			if (frameIdx) out << std::string(padLen + space, ' ');

			if (row < data_count(frame))
			{
				out << text;
				lastFrameLinesLength[row] = uint32_t(text.length());
			}
			else
			{
				lastFrameLinesLength[row] = rowLen;
			}
		}
		out << std::endl;
	}
}

PrettyPrinter::Cursor PrettyPrinter::getCursor (uint32_t cursor)
{
	uint32_t cursorCount = data_count(m_cursors);
	if ((cursor + 1) > cursorCount)
	{
		for (uint32_t i = cursorCount; i <= cursor; ++i)
			m_cursors.emplace_back();
	}
	return m_cursors[cursor];
}

PrettyPrinter::Cursor PrettyPrinter::merge(
	Cursor out, uint32_t space, bool fitIndividualColumn) const
{
	std::vector<uint32_t> mask(m_cursors.size());
	std::iota(mask.begin(), mask.end(), 0u);
	return merge(mask, out, space, fitIndividualColumn);
}

PrettyPrinter::Cursor PrettyPrinter::merge (
	add_cref<std::vector<uint32_t>> cursorMask,
	Cursor out, uint32_t space, bool fitIndividualColumn) const
{
	for (const uint32_t mask : cursorMask)
	{
		if (mask != INVALID_UINT32)
			ASSERTMSG(mask < data_count(m_cursors),
				" mask = ", mask, ", m_cursors.size() = ", data_count(m_cursors));
	}

	const auto frameCount = data_count(m_cursors);
	std::vector<strings> frames(frameCount);
	for (uint32_t i = 0u; i < frameCount; ++i)
	{
		std::string line;
		std::istringstream cursor(m_cursors[i].str());
		while (std::getline(cursor, line))
		{
			frames[i].emplace_back(std::move(line));
		}
	}

	std::vector<std::vector<uint32_t>> segments;
	std::vector<uint32_t> segment;
	segment.reserve(cursorMask.size());

	for (const uint32_t mask : cursorMask) {
		if (mask == INVALID_UINT32) {
			if (false == segment.empty()) {
				segments.push_back(segment);
				segment.clear();
			}
		}
		else segment.push_back(mask);
	}
	if (false == segment.empty())
		segments.push_back(segment);

	uint32_t maxLineLength = 0u;
	uint32_t maxLineLengthPerFrame = 0u;
	std::vector<uint32_t> maxLineLengthPerFrames(frameCount);
	for (uint32_t i = 0u; i < frameCount; ++i)
	{
		maxLineLengthPerFrame = 0u;
		for (add_cref<std::string> line : frames[i])
			maxLineLengthPerFrame = std::max(uint32_t(line.length()), maxLineLengthPerFrame);
		maxLineLengthPerFrames[i] = maxLineLengthPerFrame;
		maxLineLength = std::max(maxLineLength, maxLineLengthPerFrame);
	}
	for (add_cref<std::vector<uint32_t>> sequence : segments)
		printFrames(out, sequence, frames, maxLineLengthPerFrames, maxLineLength, space, fitIndividualColumn);

	return out;
}

PrettyPrinter::Cursor PrettyPrinter::selfTest (Cursor str) const
{
	PrettyPrinter p;
	ZFormatInfoIterator fit;
	const uint32_t rows = 10u;
	const uint32_t blocks = fit.getCount() / rows;
	std::vector<uint32_t> mask;

	if (true)
	{
		Cursor c0 = p.getCursor(0);
		c0 << "Alina" << std::endl;
		c0 << "nie ma" << std::endl;
		c0 << "kociaka" << std::endl;
		c0 << "ABCDEFGHIJKLMN" << std::endl;
		Cursor c1 = p.getCursor(1);
		c1 << "Barbara" << std::endl;
		c1 << "zjada teraz" << std::endl;
		c1 << "konika polnego" << std::endl;
		c1 << "01234567801234" << std::endl;
		mask.push_back(0);
		mask.push_back(1);
		return p.merge(mask, str);
	}

	for (uint32_t b = 0u; b < blocks; ++b)
	{
		Cursor c = p.getCursor(b);
		for (uint32_t r = 0u; r < rows; ++r)
		{
			if (r & 1)
			{
				if ((r % 3u) == 0u)
					c << std::endl;
				else
				{
					fit.next();
					c << fit.name << std::endl;
				}
			}
			else
			{
				if ((r % 4u) == 0u)
					c << std::endl;
				else
				{
					fit.next();
					c << fit.name << std::endl;
				}
			}
		}
		mask.push_back(((b % 5) == 0u) ? INVALID_UINT32 : b);
	}
	return p.merge(mask, str);
}

}
