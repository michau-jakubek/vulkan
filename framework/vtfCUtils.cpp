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

template<class T> T fromText(const std::string& text, const T& defResult, bool& status)
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
template int fromText<int>(const std::string& text, const int& defResult, bool& status);
template float fromText<float>(const std::string& text, const float& defResult, bool& status);
template std::string fromText<std::string>(const std::string& text, const std::string& defResult, bool& status);
template uint32_t fromText<uint32_t>(const std::string& text, const uint32_t& defResult, bool& status);
template<> bool fromText<bool>(const std::string& text, const bool& defResult, bool& status)
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

std::string readFile (const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file! " + filename);
	}

	size_t fileSize = (size_t) file.tellg();
	std::string			buffer(fileSize, ' ');

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

uint32_t readFile(const fs::path& path, std::vector<unsigned char>& buffer)
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

std::string subst_variables(const std::string& templateStr, const vtf::string_to_string_map& variables)
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

bool containsAllString (const vtf::strings& all, const vtf::strings& range)
{
	bool contains = true;
	for (auto i = all.begin(); contains && i != all.end(); ++i)
	{
		contains = containsString(*i, range);
	}
	return contains;
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
				ss << c;
			}
		}
		cross_platform_pclose(pp);
		result = ss.str();
		status = length > 0;
	}
	else
	{
		status = false;
	}
	return result;
}

std::string getRealPath(const char* path, bool& status)
{
	const auto cmd = std::string("realpath ") + path;
	std::string realpath = captureSystemCommandResult(cmd.c_str(), status);
	return status ? realpath : std::string(path);
}

template<class T> bool between(const T& paramValue, const T& paramMin, const T& paramMax)
{
	return (paramMin <= paramValue && paramValue <= paramMax);
}

template bool between(const int& paramValue, const int& paramMin, const int& paramMax);
template bool between(const float& paramValue, const float& paramMin, const float& paramMax);
template bool between(const uint32_t& paramValue, const uint32_t& paramMin, const uint32_t& paramMax);

} // namespace vtf
