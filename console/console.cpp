#include <algorithm>
#include <fstream>
#include <functional>	// std::reference_wrapper
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>
#if SYSTEM_OS_LINUX == 1
 #include <termios.h>
 #include <sys/ioctl.h>
 #include <unistd.h>	// read()
#else
 #include <Windows.h>
#endif

#include "console.hpp"

#if SYSTEM_OS_LINUX == 1

#define K_DELETE    0x7E335B1B
#define PG_UP       0x7E355B1B
#define PG_DOWN     0x7E365B1B
#define K_INSERT    0x7E325B1B

/* Chord keys
#define CTRL_LEFT   0x3B315B1B  0x4435
#define SHFT_LEFT   0x3B315B1B  0x4432
#define STRL_RIGHT  0x3B315B1B  0x4335
#define SHFT_RIGHT  0x3B315B1B  0x4332
*/

#define UP_ARROW    0x00415B1B
#define DN_ARROW    0x00425B1B
#define RT_ARROW    0x00435B1B
#define LT_ARROW    0x00445B1B

#define K_HOME      0x00485B1B
#define K_END       0x00465B1B

#else

#define K_DELETE    VK_DELETE
#define PG_UP       VK_PRIOR
#define PG_DOWN     VK_NEXT
#define K_INSERT    VK_INSERT

/* Chord keys
#define CTRL_LEFT   0x3B315B1B  0x4435
#define SHFT_LEFT   0x3B315B1B  0x4432
#define STRL_RIGHT  0x3B315B1B  0x4335
#define SHFT_RIGHT  0x3B315B1B  0x4332
*/

#define UP_ARROW    VK_UP
#define DN_ARROW    VK_DOWN
#define RT_ARROW    VK_RIGHT
#define LT_ARROW    VK_LEFT

#define K_HOME      VK_HOME
#define K_END       VK_END

#endif

typedef std::integral_constant<int, K_DELETE    > KeyDelete;
typedef std::integral_constant<int, PG_UP       > KeyPageUp;
typedef std::integral_constant<int, PG_DOWN     > KeyPageDown;
typedef std::integral_constant<int, K_INSERT    > KeyInsert;
typedef std::integral_constant<int, UP_ARROW    > KeyUpArrow;
typedef std::integral_constant<int, DN_ARROW    > KeyDownArrow;
typedef std::integral_constant<int, RT_ARROW    > KeyRightArrow;
typedef std::integral_constant<int, LT_ARROW    > KeyLeftArrow;
typedef std::integral_constant<int, K_HOME      > KeyHome;
typedef std::integral_constant<int, K_END       > KeyEnd;

#if SYSTEM_OS_LINUX == 1

#define K_StartOfHeading			0x01
#define K_StartOfText				0x02
#define K_EndOfText				    0x03
#define K_EndOfTransmission		    0x04
#define K_Enquiry					0x05
#define K_Acknowledge				0x06
#define K_Bell					    0x07
#define K_Backspace				    0x08
#define K_Backspace2				0x7F
#define K_HorizontalTab			    0x09
#define K_LineFeed				    0x0A
#define K_VerticalTab				0x0B
#define K_FormFeed				    0x0C
#define K_CarriageReturn			0x0D
#define K_ShiftOut				    0x0E
#define K_ShiftIn					0x0F
#define K_DataLinkEscape			0x10
#define K_DeviceControl1			0x11	// XON
#define K_DeviceControl2			0x12
#define K_DeviceControl3			0x13	// XOFF
#define K_DeviceControl4			0x14
#define K_NegativeAcknowledge		0x15
#define K_SynchronousIdle			0x16
#define K_EndOfTransmissionBlock	0x17
#define K_Cancel					0x18
#define K_EndOfMedium				0x19
#define K_Substitute				0x1A
#define K_Escape					0x1B
#define K_FileSeparator			    0x1C
#define K_GroupSeparator			0x1D
#define K_RecordSeparator			0x1E
#define K_UnitSeparator			    0x1F
#define K_WhiteSpace				0x20

#else

#define K_Backspace         VK_BACK
#define K_Backspace2        VK_F16
#define K_HorizontalTab     VK_TAB
#define K_LineFeed          0xA
#define K_CarriageReturn    VK_RETURN
#define K_Escape            VK_ESCAPE
#define K_WhiteSpace        VK_SPACE

#endif

//  typedef std::integral_constant<int, K_StartOfHeading			> StartOfHeading;
//  typedef std::integral_constant<int, K_StartOfText				> StartOfText;
//  typedef std::integral_constant<int, K_EndOfText				    > EndOfText;
//  typedef std::integral_constant<int, K_EndOfTransmission		    > EndOfTransmission;
//  typedef std::integral_constant<int, K_Enquiry					> Enquiry;
//  typedef std::integral_constant<int, K_Acknowledge				> Acknowledge;
//  typedef std::integral_constant<int, K_Bell					    > Bell;
typedef std::integral_constant<int, K_Backspace				    > Backspace;
typedef std::integral_constant<int, K_Backspace2				> Backspace2;
typedef std::integral_constant<int, K_HorizontalTab			    > HorizontalTab;
typedef std::integral_constant<int, K_LineFeed				    > LineFeed;
//  typedef std::integral_constant<int, Linux_VerticalTab				> VerticalTab;
//  typedef std::integral_constant<int, Linux_FormFeed				    > FormFeed;
typedef std::integral_constant<int, K_CarriageReturn			> CarriageReturn;
//  typedef std::integral_constant<int, Linux_ShiftOut				    > ShiftOut;
//  typedef std::integral_constant<int, Linux_ShiftIn					> ShiftIn;
//  typedef std::integral_constant<int, Linux_DataLinkEscape			> DataLinkEscape;
//  typedef std::integral_constant<int, Linux_DeviceControl1			> DeviceControl1;
//  typedef std::integral_constant<int, Linux_DeviceControl2			> DeviceControl2;
//  typedef std::integral_constant<int, Linux_DeviceControl3			> DeviceControl3;
//  typedef std::integral_constant<int, Linux_DeviceControl4			> DeviceControl4;
//  typedef std::integral_constant<int, Linux_NegativeAcknowledge		> NegativeAcknowledge;
//  typedef std::integral_constant<int, Linux_SynchronousIdle			> SynchronousIdle;
//  typedef std::integral_constant<int, Linux_EndOfTransmissionBlock	> EndOfTransmissionBlock;
//  typedef std::integral_constant<int, Linux_Cancel					> Cancel;
//  typedef std::integral_constant<int, Linux_EndOfMedium				> EndOfMedium;
//  typedef std::integral_constant<int, Linux_Substitute				> Substitute;
//  typedef std::integral_constant<int, Linux_Escape					> Escape;
//  typedef std::integral_constant<int, Linux_FileSeparator			    > FileSeparator;
//  typedef std::integral_constant<int, Linux_GroupSeparator			> GroupSeparator;
//  typedef std::integral_constant<int, Linux_RecordSeparator			> RecordSeparator;
//  typedef std::integral_constant<int, Linux_UnitSeparator			    > UnitSeparator;
typedef std::integral_constant<int, K_Escape                    > KeyEscape;
typedef std::integral_constant<int, K_WhiteSpace				> WhiteSpace;

namespace icon {

template<class I>
bool isEmptyString(I first, I last)
{
    while (first != last) {
        if (*first != ' ')
            return false;
        ++first;
    }
    return true;
}

RawConsole::RawConsole(add_ref<std::ostream> str, size_t maxHistoryCount)
    : maxHistory(maxHistoryCount)
    , tabw(4)
    , tabs(tabw, static_cast<char>(WhiteSpace::value))
    , pos_buff(0)
    , pos_hist(0)
    , pos_curr(0)
    , hist(1)
    , buff(hist.at(pos_hist).second)
    , m_debugPipe() //"DEBUG")  // watch -n 1 -d cat DEBUG
    , processed(false)
    , prefix()
#if SYSTEM_OS_LINUX == 1
    , stream(str)
#else
    , cmdStream()
    , stream(cmdStream)
#endif
    , size(getSize())
{
    UNREF(str);
    enableRaw();
}

RawConsole::~RawConsole()
{
    disableRaw();
}

void RawConsole::writeDebugPipe(int v) const
{
    static char ib[20];
    static char hb[20];
    if (m_debugPipe.is_open())
    {
#if SYSTEM_OS_LINUX == 1
        std::sprintf(ib, "%d", v);
        std::sprintf(hb, "%X", v);
#else
        sprintf_s(ib, "%d", v);
        sprintf_s(hb, "%X", v);
#endif
        writeDebugPipe(std::string(ib));
        writeDebugPipe(std::string(hb));
    }
}

void RawConsole::writeDebugPipe(const std::string& s) const
{
    if (m_debugPipe.is_open())
    {
        m_debugPipe << s << std::endl;
        m_debugPipe.flush();
    }
}

void RawConsole::writeDebugPipe(const std::vector<char>& s) const
{
    if (m_debugPipe.is_open())
    {
        m_debugPipe << std::string(s.begin(), s.end()) << std::endl;
        m_debugPipe.flush();
    }
}

int RawConsole::readc(add_ref<int> count, add_ref<int> rsv) const
{
    int chr = 0;
#if SYSTEM_OS_LINUX
    UNREF(rsv);
    auto readed = read(STDIN_FILENO, &chr, sizeof(chr));
    count = static_cast<int>(readed);
#else
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD ir{};
    DWORD readed{};
    if (0 != rsv)
    {
        chr = rsv;
        rsv = 0;
        count = 1;
    }
    else
    {
        count = 0;
        WaitForSingleObject(hConsole, 3000);
        BOOL status = ReadConsoleInput(hConsole, &ir, 1, &readed);
        if (status && (0 < readed) && (KEY_EVENT == ir.EventType))
        {
            if (ir.Event.KeyEvent.bKeyDown)
            {
                chr = static_cast<int>(ir.Event.KeyEvent.uChar.AsciiChar);
                rsv = static_cast<int>(ir.Event.KeyEvent.wVirtualKeyCode);
                if (rsv == 0) chr = '*';
            }
            else
            {
                chr = rsv = 0;
            }
        }
        count = static_cast<int>(readed);
    }
#endif
    return chr;
}

void RawConsole::enableRaw()
{
#if SYSTEM_OS_LINUX == 1
    typedef decltype(after.c_lflag) c_lflag_t;
    tcgetattr(STDOUT_FILENO, &before);
    after = before;
    after.c_lflag &= ~static_cast<c_lflag_t>(ICANON);
    after.c_lflag &= ~static_cast<c_lflag_t>(ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &after);
#else
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    BOOL status = GetConsoleMode(hConsole, &before);
    static_cast<void>(status);
    after = before;
    after &= ~ENABLE_LINE_INPUT;
    after &= ~ENABLE_ECHO_INPUT;
    after &= ~ENABLE_LINE_INPUT;
    //after |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(hConsole, after);
#endif
}

void RawConsole::disableRaw()
{
#if SYSTEM_OS_LINUX == 1
    tcsetattr(0, TCSANOW, &before);
#else
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hConsole, before);
#endif
}

std::pair<size_t,size_t> RawConsole::getSize()
{
#if SYSTEM_OS_LINUX == 1
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    // TODO: col,row
    return { w.ws_row, w.ws_col };
#else
    return {};
#endif
}

size_t tabulised_string_width(const std::string& s, size_t tabw)
{
    size_t width = 0;
    auto beg = s.begin();
    auto end = s.end();
    while (beg != end)
    {
        width += *beg++ == static_cast<char>(HorizontalTab::value) ? tabw : 1;
    }
    return width;
}

std::string RawConsole::stringFromCursor() const
{
    return std::string();
}

std::vector<char> RawConsole::fromString(add_cref<std::string> line) const
{
    std::vector<char> v;

    const size_t linew = tabulised_string_width(line, tabw);
    v.reserve(linew);

    auto lbeg = line.begin();
    auto lend = line.end();
    while (lbeg != lend)
    {
        const char c = *lbeg++;
        if (c == static_cast<char>(HorizontalTab::value))
            v.insert(v.end(), tabs.begin(), tabs.end());
        else v.emplace_back(c);
    }

    return v;
}

size_t RawConsole::historyAdd(const std::string& s)
{
    auto v = fromString(s);
    hist.back().first = v.size();
    hist.back().second = std::move(v);
    hist.resize(hist.size()+1);
    buff = hist.back().second;
    pos_hist = hist.size() - 1;
    pos_buff = 0;
    pos_curr = 0;
    return 0;
}

size_t RawConsole::load(const std::string& fileName)
{
    size_t res = 0;
    std::ifstream f(fileName);
    if (f.is_open())
    {
        hist.clear();
        std::string line;
        while (std::getline(f, line))
        {
            if (line.length() == 0
                || isEmptyString(line.begin(), line.end()))
                continue;

            auto v = fromString(line);
            hist.emplace_back(v.size(), std::move(v));
            if (++res > maxHistory)
            {
                hist.erase(hist.begin());
                --res;
            }
        }
        pos_hist = hist.size();
        hist.resize(pos_hist + 1);
        buff = hist.at(pos_hist).second;
        pos_buff = 0;
        pos_curr = 0;
    }
    return res;
}

bool RawConsole::save(const std::string& fileName) const
{
    bool res = false;
    std::ofstream f(fileName, std::ios::trunc);
    if (f.is_open())
    {
        const size_t ii = hist.size();
        for (size_t i = 0; i < ii; ++i)
        {
            const std::vector<char>& v = hist.at(i).second;
            f << std::string(v.begin(), std::next(v.begin(), static_cast<long>(hist.at(i).first)));
            if (ii - i > 1) f << std::endl;
        }
        f.close();
        res = true;
    }
    return res;
}

size_t RawConsole::historyCount() const
{
    return hist.size();
}

void RawConsole::updateModified()
{
    if (hist.size() - pos_hist != 1)
    {
        auto beg = buff.get().begin();
        std::vector<char> v(beg, std::next(beg, static_cast<int>(pos_buff)));
        std::pair<size_t,std::vector<char>>& last(hist.back());
        last.second = std::move(v);
        last.first = pos_buff;
        buff = last.second;
        pos_hist = hist.size() - 1;
    }
}

void RawConsole::listen(std::function<bool(int chr, int c_chr)> onKey)
{
    int c_chr = 0;
    int ctrl = 0;
    bool doContinue = true;
    do
    {
        int chr = readc(c_chr, ctrl);
        doContinue = onKey(chr, c_chr);
    }
    while (doContinue);
}

void RawConsole::process(ON_NEW_LINE const& onNewLine, _uUser user)
{
    stream << prefix;
    // TODO std::flush(stream);
    stream.flush();

    int c_chr = 0, rsv = c_chr;
    bool continueAfterNewLine = true;
#if SYSTEM_OS_LINUX == 1
    rsv = 1;
#endif
    const int cmprsv = rsv;

    while (continueAfterNewLine && 0 <= c_chr)
    {
        processed = false;
        int chr = readc(c_chr, rsv);
        writeDebugPipe(chr);
        switch (chr)
        {
        case 0: break;

        case Backspace::value:
        case Backspace2::value:
            processed = true;
            if (pos_buff > 0 && pos_curr > 0 && rsv == cmprsv)
            {
                updateModified();

                --pos_buff;
                --pos_curr;

                auto end = buff.get().begin();
                auto beg = std::next(end, static_cast<int>(pos_curr));
                beg = buff.get().erase(beg);
                end = buff.get().end();
                stream
                    << static_cast<char>(Backspace::value)
                    << std::string(beg, end) << static_cast<char>(WhiteSpace::value)
                    << std::string(pos_buff - pos_curr + 1, static_cast<char>(Backspace::value));
            }
            break;

        case KeyDelete::value:
            processed = true;
            if (pos_buff > 0 && pos_curr < pos_buff)
            {
                updateModified();

                --pos_buff;

                auto end = buff.get().begin();
                auto beg = std::next(end, static_cast<int>(pos_curr));
                beg = buff.get().erase(beg);
                end = std::next(beg, static_cast<int>(pos_buff - pos_curr));
                end = buff.get().end();
                
                stream
                    << std::string(beg, end) << static_cast<char>(WhiteSpace::value)
                    << std::string(pos_buff - pos_curr + 1, static_cast<char>(Backspace::value));
            }
            break;

        case LineFeed::value:
        case CarriageReturn::value:
            processed = true;
            {
                auto beg = buff.get().begin();
                if (onNewLine)
                {
                    (*onNewLine)(*this, user,
                        std::string(beg, std::next(beg, static_cast<int>(pos_buff))), continueAfterNewLine);
                }

                // At the last, currently being written item
                if (hist.size() - pos_hist == 1)
                {
                    hist.at(pos_hist).first = pos_buff;
                    if (false == isEmptyString(beg, std::next(beg, static_cast<int>(pos_buff))))
                    {
                        hist.resize(hist.size() + 1);
                        ++pos_hist;
                    }
                }
                // At the each other item except penultimate
                else if (hist.size() - pos_hist > 2)
                {
                    std::vector<char> v(beg, std::next(beg, static_cast<int>(pos_buff)));
                    hist.back().second = std::move(v);
                    hist.back().first = pos_buff;
                    hist.resize(hist.size() + 1);
                    pos_hist = hist.size() - 1;
                }
                // At the penultimate item (avoid multiply last item copies)
                else
                {
                    pos_hist = hist.size() - 1;
                }

                buff = hist.at(pos_hist).second;
                pos_buff = hist.at(pos_hist).first;
                pos_curr = pos_buff;

                stream << static_cast<char>(LineFeed::value) << static_cast<char>(CarriageReturn::value);
                if (continueAfterNewLine) stream << prefix;
            }
            break;

        case HorizontalTab::value:
            processed = true;

            updateModified();

            buff.get().insert(std::next(buff.get().begin(),
                static_cast<int>(pos_curr)), tabs.begin(), tabs.end());
            stream << tabs;

            pos_buff += tabw;
            pos_curr += tabw;

            if (pos_curr < pos_buff)
            {
                auto beg = buff.get().begin();
                stream
                    << std::string(std::next(beg, static_cast<int>(pos_curr)),
                        std::next(beg, static_cast<int>(pos_buff)))
                    << std::string(pos_buff - pos_curr, static_cast<char>(Backspace::value));
            }
            break;

        case KeyUpArrow::value:
        case KeyDownArrow::value:
            processed = true;
            if ((KeyUpArrow::value == chr && pos_hist > 0) || (KeyDownArrow::value == chr && (pos_hist + 1) < hist.size()))
            {
                size_t erase_count;
                // Amount of characters to wipe equals to current buffer length.
                hist.at(pos_hist).first = erase_count = pos_buff;

                if (KeyDownArrow::value == chr)
                    pos_hist += 1;
                else pos_hist -= 1;
                pos_buff = hist.at(pos_hist).first;
                pos_curr = pos_buff;
                buff = hist.at(pos_hist).second;
                auto begin = buff.get().begin();
                stream
                    // go to the start of line
                    << static_cast<char>(CarriageReturn::value)
                    // erase previous terminal content
                    << std::string(erase_count + prefix.length(), static_cast<char>(WhiteSpace::value))
                    // go to the start of line again
                    << static_cast<char>(CarriageReturn::value)
                    // print a prefix
                    << prefix
                    // print changed line
                    << std::string(begin, std::next(begin, static_cast<int>(pos_buff)));
            }
            break;

        case KeyLeftArrow::value:
            processed = true;
            if (pos_curr > 0)
            {
                --pos_curr;
                stream << static_cast<char>(Backspace::value);
            }
            break;

        case KeyHome::value:
            processed = true;
            if (pos_curr > 0)
            {
                stream << std::string(pos_curr, static_cast<char>(Backspace::value));
                pos_curr = 0;
            }
            break;

        case KeyRightArrow::value:
            processed = true;
            if (pos_curr < pos_buff)
            {
                stream << buff.get()[pos_curr++];
            }
            break;

        case KeyEnd::value:
            processed = true;
            if (pos_curr < pos_buff)
            {
                auto beg = buff.get().begin();
                stream << std::string(std::next(beg, static_cast<int>(pos_curr)),
                    std::next(beg, static_cast<int>(pos_buff)));
                pos_curr = pos_buff;
            }
            break;

        default:
            if ((1 == c_chr) && (0 != rsv))
            {
                processed = true;

                updateModified();

                auto beg = buff.get().begin();
                buff.get().insert(std::next(beg, static_cast<int>(pos_curr)), static_cast<char>(chr));

                stream << static_cast<char>(chr);
                ++pos_curr;
                ++pos_buff;

                if (pos_curr < pos_buff)
                {
                    size_t tmp = pos_curr;
                    while (tmp < pos_buff) stream << buff.get()[tmp++];
                    stream << std::string(pos_buff - pos_curr, static_cast<char>(Backspace::value));
                }
            }
        }

        if (processed)
        {
            // TODO std::flush(stream);
            stream.flush();
        }
    }
}

std::pair<bool, std::string> Console::check_input_and_reset(std::istream& s)
{
    std::pair<bool, std::string> res(true, std::string());
    if (s.fail())
    {
        std::vector<std::string> chunks;
        s.clear();
        s >> chunks;
        res.first = false;
        res.second  = std::accumulate(chunks.begin(), chunks.end(), std::string());
        //s.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return res;
}

////////////// iconstream ////////////
iconstream::iconstream(std::ostream& ostream, const std::string& prompt, size_t maxHistory)
    : std::istringstream()
    , Console(ostream, maxHistory)
{
    this->prefix = prompt;
}

void iconstream::onNewLine(const RawConsole&, _uUser user, const std::string& line, bool& doContinue)
{
	user.pIconstream->clear();
	user.pIconstream->str(line);
    doContinue = false;
}
void iconstream::read()
{
	process(onNewLine, this);
}
std::pair<bool, std::string> iconstream::check_input_and_reset() {
    return Console::check_input_and_reset(*this);
}

std::istream& operator>>(std::istream& s, std::vector<std::string>& items)
{
	items.clear();
	do
	{
		std::string item;
		s >> std::quoted(item);
		if (false == item.empty()) items.push_back(item);
	}
	while ((false == s.eof()) && s.peek() != '\n');

	return s;
}

std::istream& operator>>(std::istream& s, add_ref<std::pair<std::string, std::string>> items)
{
    s >> std::quoted(items.first) >> std::noskipws;
    items.second.assign(std::istream_iterator<char>(s), (std::istream_iterator<char>()));
    if (items.second.length() > 0)
    {
        items.second.insert(items.second.begin(), ' ');
    }
    s >> std::skipws;
    return s;
}

} // namespace icon

