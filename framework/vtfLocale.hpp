#ifndef __VTF_LOCALE_HPP_INCLUDED__
#define __VTF_LOCALE_HPP_INCLUDED__

#include "vtfZDeletable.hpp"
#include <locale>
#include <initializer_list>
#include <iostream>
#include <vector>

namespace vtf
{

enum class Locales
{
	Unknown,
	pl_PL
};

std::locale makeLocale (add_cref<Locales> locales);

struct Diacritics
{
	virtual auto encode		(char in)		const -> wchar_t = 0;
	virtual auto decode		(wchar_t in)	const -> wchar_t = 0;
	virtual auto getName	()				const -> add_cptr<char> = 0;
	virtual auto getSize	()				const -> int = 0;
};

auto	getDiacritics			(add_cref<Locales> locales) -> std::vector<add_cptr<Diacritics>>;
auto	findDiacriticsByName	(add_cref<Locales> locales,
								 add_cref<std::string> codePage) -> add_cptr<Diacritics>;
int		findBestDiacritics		(add_cref<std::vector<add_cptr<Diacritics>>>,
								 std::initializer_list<char>	encoded,
								 std::initializer_list<wchar_t>	decoded);

bool	isPolishLocaleInstalled ();

struct WcharOrChar
{
	WcharOrChar(char c) : character(c) {}
	WcharOrChar(wchar_t c) : character(c) {}
	friend add_ref<std::ostream> operator<< (add_ref<std::ostream>, add_cref<WcharOrChar>);
	wchar_t	character;
};

void	writeExampleText		(std::ostream&			str,
								const std::string&		text,
								add_cref<Locales>		locales,
								add_cptr<Diacritics>	d);

} // namespace vtf

#endif // __VTF_LOCALE_HPP_INCLUDED__
