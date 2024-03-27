#include <sstream>
#include <stdio.h>
#include <stdlib.h>

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
		std::transform(str.begin(), str.end(), str.begin(),
					   [](const char c) { return static_cast<char>(std::toupper(c)); });
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
			throw std::runtime_error("failed to open file! " + filename);
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

uint32_t readFile (const fs::path& path, std::vector<unsigned char>& buffer)
{
	if (!fs::exists(path)) return INVALID_UINT32;
	const uint32_t length = (uint32_t)fs::file_size(path);
	std::ifstream s(path.c_str(), std::ios::binary);
	if (!s.is_open()) return INVALID_UINT32;
	buffer.resize(length);
	std::copy(std::istreambuf_iterator<char>(s), std::istreambuf_iterator<char>(), buffer.begin());
	s.close();
	return length;
}

std::string subst_variables (const std::string& templateStr, const vtf::string_to_string_map& variables)
{
	std::string result(templateStr);
	for (auto kv = variables.begin(); kv != variables.end(); ++kv)
	{
		std::string var("${" + kv->first + '}');
		std::string::size_type pos = result.find(var);
		ASSERTION(std::string::npos != pos);
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

strings mergeStrings (const strings& a, const strings& b)
{
	strings c(a);
	c.insert(c.end(), b.begin(), b.end());
	return c;
}

strings mergeStringsDistinct (const strings& a, const strings& b)
{
	strings c(a);
	for (const auto& s : b)
	{
		auto i = std::find(a.begin(), a.end(), s);
		if (a.end() == i) c.emplace_back(s);
	}
	return c;
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

} // namespace vtf
