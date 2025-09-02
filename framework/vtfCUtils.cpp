#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string_view>

#include "vtfZUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeletable.hpp"

#ifndef _MSC_VER
  #define cross_platform_pclose(p0) pclose(p0)
  #define cross_platform_popen(p0,p1) popen((p0),(p1))
#else
  #define cross_platform_pclose(p0) _pclose(p0)
  #define cross_platform_popen(p0,p1) _popen((p0),(p1))
#endif

namespace vtf
{

Boolean boolean (bool value, bool yesno) { return Boolean{ value, yesno }; }
std::ostream& operator<< (std::ostream& str, const Boolean& value)
{
	if (value.yesno)
		return (str << (value.value ? "Yes": "No"));
	return (str << std::boolalpha << value.value << std::noboolalpha);
}

template<class T> T fromText (const std::string& text, const T& defResult, bool& status)
{
	T s_result {};
	std::stringstream s;
	s << text;
	s >> s_result;
	if (!s.fail())
	{
		status = true;
		return s_result;
	}
	status = false;
	return defResult;
}
template std::string	fromText<std::string>	(const std::string& text, const std::string&	defResult, bool& status);
template<> float		fromText<float>			(const std::string& text, const float&			defResult, bool& status)
{
	float result{ defResult };
	std::stringstream s;
	s << text;
	std::string_view sw(text);
	if (sw.substr(0, 2) == std::string_view("0x"))
		s >> std::hexfloat >> result;
	else s >> result;
	if (!s.fail())
	{
		status = true;
		return result;
	}
	status = false;
	return defResult;
}
template<> uint64_t		fromText<uint64_t>		(const std::string& text, const uint64_t&		defResult, bool& status)
{
	uint64_t result { defResult };
	std::stringstream s;
	s << text;
	std::string_view sw(text);
	if (sw.substr(0,2) == std::string_view("0x"))
		s >> std::hex >> result;
	else s >> result;
	if (!s.fail())
	{
		status = true;
		return result;
	}
	status = false;
	return defResult;
}
/*
template unsigned long long	fromText<
		typename std::enable_if<!std::is_same<unsigned long long, uint64_t>::value, unsigned long long>::type>
			(const std::string& text, const unsigned long long&	defResult, bool& status);
template unsigned long	fromText<
		typename std::enable_if<!std::is_same<unsigned long, uint32_t>::value, unsigned long>::type>
			(const std::string& text, const unsigned long&	defResult, bool& status);
*/
template<> uint32_t		fromText<uint32_t>		(const std::string& text, const uint32_t&		defResult, bool& status)
{
	return static_cast<uint32_t>(fromText<uint64_t>(text, static_cast<uint64_t>(defResult), status) & 0xFFFFFFFF);
}
template<> int32_t		fromText<int32_t>		(const std::string& text, const int32_t&		defResult, bool& status)
{
	return static_cast<int32_t>(
						make_signed(fromText<uint64_t>(text,
						static_cast<uint64_t>(make_unsigned(defResult)),
												status)) & 0xFFFFFFFF);
}

template<> bool			fromText<bool>			(const std::string& text, const bool&			defResult, bool& status)
{
	int logic = fromText<int>(text, defResult, status);
	if (status)
	{
		return (logic != 0);
	}

	std::string str {};
	std::stringstream s;
	s << text;
	s >> str;
	if (!s.fail())
	{
		status = true;
		toUpper(str);
		return str == "TRUE";
	}
	status = false;
	return defResult;
}

std::string readFile (const std::string& filename, bool* status)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		if (nullptr == status || !(*status))
			throw std::runtime_error("failed to open file! " + filename + "\n");
		else
		{
			*status = false;
			return std::string();
		}
	}

	long fileSize	= static_cast<long>(file.tellg());
	std::string		buffer(make_unsigned(fileSize), ' ');

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	if (nullptr != status) *status = true;

	return buffer;
}

uint32_t readFile (add_cref<fs::path> path, add_ref<std::vector<char>> buffer)
{
	if (!fs::exists(path)) return INVALID_UINT32;
	const uint32_t length = (uint32_t)fs::file_size(path);
    std::basic_ifstream<char> s(path.c_str(), std::ios::binary);
	if (!s.is_open()) return INVALID_UINT32;
	buffer.resize(length);
    std::copy(std::istreambuf_iterator<char>(s), std::istreambuf_iterator<char>(), buffer.begin());
	s.close();
	return length;
}

std::vector<uint8_t> base64_decode (add_cref<std::vector<char>> input)
{
	static const std::string base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	std::vector<uint8_t> output;
	output.reserve((input.size() / 4u) * 3u);
	int val = 0;
	int bits = -8;

	for (char c : input) {
		if (std::isspace(c)) continue; // ignore white spaces and new lines
		if (c == '=') break;           // padding as the end of data
		ASSERTMSG(std::isalnum(c) || c == '+' || c == '/', "Invalid Base64 character");

		val = (val << 6) + int(base64_chars.find(c));
		bits += 6;

		if (bits >= 0) {
			output.push_back(uint8_t((val >> bits) & 0xFF));
			bits -= 8;
		}
	}

	return output;
}

std::string subst_variables (const std::string& templateStr, const vtf::string_to_string_map& variables, bool validateVariableExistence)
{
	std::string result(templateStr);
	for (auto kv = variables.begin(); kv != variables.end(); ++kv)
	{
		std::string var("${" + kv->first + '}');
		std::string::size_type pos = result.find(var);
		if (validateVariableExistence) ASSERTION(std::string::npos != pos);
		while (std::string::npos != pos)
		{
			result.replace(pos, var.length(), kv->second);
			pos = result.find(var, (pos + kv->second.length()));
		}
	}
	return result;
}

bool containsString (const std::string& s, const vtf::strings& list)
{
	const auto p = std::find(list.begin(), list.end(), s);
	return p != list.end();
}

bool containsString(const vtf::strings& list, const std::string& s)
{
	return containsString(s, list);
}

bool containsAllStrings (const vtf::strings& range, const vtf::strings& all)
{
	bool contains = true;
	for (auto i = all.begin(); contains && i != all.end(); ++i)
	{
		contains &= containsString(*i, range);
	}
	return contains;
}

bool containsAnyString (const vtf::strings& range, const vtf::strings& include)
{
	for (auto i = include.begin(); i != include.end(); ++i)
	{
		if (containsString(*i, range))
			return true;
	}
	return false;
}

uint32_t removeStrings (const vtf::strings& strs, vtf::strings& list)
{
	uint32_t count = 0;
	for (auto i = strs.begin(); i != strs.end(); ++i)
	{
		if (auto p = std::find(list.begin(), list.end(), *i); p != list.end())
		{
			list.erase(p);
			++count;
		}
	}
	return count;
}

void distinctStrings (add_ref<strings> target)
{
	std::sort(target.begin(), target.end());
	target.erase(std::unique(target.begin(), target.end()), target.end());
}

strings distinctStrings (add_cref<strings> set)
{
	strings res(set);
	distinctStrings(res);
	return res;
}

strings mergeStrings (add_cref<strings> target, add_cref<strings> source)
{
	strings::size_type i = 0;
	const strings::size_type ts = target.size();
	strings r(ts + source.size());
	for (; i < ts; ++i)
		r[i] = target[i];
	for (; i < r.size(); ++i)
		r[i] = source[i - ts];
	return r;
}

void mergeStringsDistinct (add_ref<strings> target, add_cref<strings> source)
{
	target.insert(target.end(), source.begin(), source.end());
	distinctStrings(target);
}

strings mergeStringsDistinct (add_cref<strings> target, add_cref<strings> source)
{
	strings res(target);
	mergeStringsDistinct(res, source);
	return res;
}

strings splitString (const std::string& delimitedString, char delimiter)
{
	strings result;
	std::stringstream ss(delimitedString);
	while (ss.good())
	{
		result.resize(result.size() + 1);
		std::getline(ss, result.back(), delimiter);
	}
	return result;
}

inline static char toLowerChar (add_cref<char> c) { return (char)std::tolower(c); }
inline static char toUpperChar (add_cref<char> c) { return (char)std::toupper(c); }

std::string	toLower (add_cref<std::string> s)
{
	std::string t(s);
	toLower(t);
	return t;
}

std::string	toUpper (add_cref<std::string> s)
{
	std::string t(s);
	toUpper(t);
	return t;
}

void toLower (add_ref<std::string> inplace)
{
	std::transform(inplace.begin(), inplace.end(), inplace.begin(), toLowerChar);
}

void toUpper (add_ref<std::string> inplace)
{
	std::transform(inplace.begin(), inplace.end(), inplace.begin(), toUpperChar);
}

void toLower (add_ref<std::string> out, add_cref<std::string> src)
{
	out.resize(src.length());
	std::transform(src.begin(), src.end(), out.begin(), toLowerChar);
}

void toUpper (add_ref<std::string> out, add_cref<std::string> src)
{
	out.resize(src.length());
	std::transform(src.begin(), src.end(), out.begin(), toUpperChar);
}

bool startswith (add_cptr<char> s, char c)
{
	const std::string_view sv(s);
	return sv.length() > 0 && sv[0] == c;
}

template<> bool startswith<char> (std::basic_string<char> s, char c)
{
	return startswith(s.c_str(), c);
}

bool compareNoCase (add_cref<std::string> a, add_cref<std::string> b)
{
	const std::string A = toUpper(a);
	const std::string B = toUpper(b);
	return A == B;
}

std::string captureSystemCommandResult (const char* cmd, bool& status, const char LF)
{
	std::string result;
	FILE* pp = cross_platform_popen(cmd, "r");
	if (pp)
	{
		char c = '\0';
		std::stringstream ss;
		uint32_t length = 0;
		while (fread(&c, 1, 1, pp)) {
			bool skip = false;
			switch (c) {
				case '\r':
					skip = true;
					break;
				case '\n': {
					if (LF != '\0')
						c = LF;
					else skip = true;
				}
			}
			if (!skip)
			{
				++length;
				UNREF(length);
				ss << c;
			}
		}
		cross_platform_pclose(pp);
		ss.flush();
		result = ss.str();
		status = true;
	}
	else
	{
		status = false;
	}
	return result;
}

std::string getRealPath (const char* path, bool& status)
{
	const auto cmd = std::string("realpath ") + path;
	std::string realpath = captureSystemCommandResult(cmd.c_str(), status);
	return status ? realpath : std::string(path);
}

template<class T> bool between (const T& paramValue, const T& paramMin, const T& paramMax)
{
	return (paramMin <= paramValue && paramValue <= paramMax);
}

template bool between (const int& paramValue, const int& paramMin, const int& paramMax);
template bool between (const float& paramValue, const float& paramMin, const float& paramMax);
template bool between (const uint32_t& paramValue, const uint32_t& paramMin, const uint32_t& paramMax);

FPS::FPS (Printer callback, const float triggerInSeconds)
	: fps		(0.0f)
	, bestFPS	(0.0f)
	, worstFPS	(99999.0f)
	, totalTime	(0.0f)
	, trigger	(triggerInSeconds)
	, frameCount(0)
	, printer	(callback)
	, lastTime	(std::chrono::high_resolution_clock::now())
	, triggerRef(trigger) {}

FPS::FPS (add_ref<float> triggerInSeconds, Printer callback)
	: fps		(0.0f)
	, bestFPS	(0.0f)
	, worstFPS	(99999.0f)
	, totalTime	(0.0f)
	, trigger	(triggerInSeconds)
	, frameCount(0)
	, printer	(callback)
	, lastTime	(std::chrono::high_resolution_clock::now())
	, triggerRef(triggerInSeconds) {}

void FPS::touch() {
	auto currentTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::ratio<1>> deltaTime = currentTime - lastTime;
	lastTime = currentTime;

	totalTime += deltaTime.count();
	frameCount++;

	if (totalTime >= triggerRef.value) {
		fps = float(frameCount) / totalTime;
		bestFPS = std::max(fps, bestFPS);
		worstFPS = std::min(fps, worstFPS);
		if (printer) printer(fps, totalTime, bestFPS, worstFPS);
		frameCount = 0;
		totalTime = 0.0f;
	}
}
void FPS::reset() {
	fps = 0.0f;
	frameCount = 0;
	totalTime = 0.0f;
	lastTime = std::chrono::high_resolution_clock::now();
	bestFPS = 0.0f;
	worstFPS = 99999.0f;
}

} // namespace vtf
