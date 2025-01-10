#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfLocale.hpp"

#include <algorithm>
#include <climits>
#include <map>
#include <string>
#include <string_view>

#if SYSTEM_OS_WINDOWS == 1
  #include <Windows.h>
  #include <WinNls.h>
#endif

namespace vtf
{

std::locale makeLocale (add_cref<Locales> locales)
{
	const char* name = nullptr;
	switch (locales)
	{
	case Locales::pl_PL:	name = "pl_PL.UTF-8";	break;
	default: ASSERTFALSE("Unknown locales");
	}
	return std::locale(name);
}

bool isPolishLocaleInstalled ()
{
	try
	{
		const std::locale loc = makeLocale(Locales::pl_PL);
		const std::string currency = std::use_facet<std::moneypunct<char, true>>(loc).curr_symbol();
		return std::string_view(currency).substr(0, 3) == std::string_view("PLN");
	}
	catch (add_ref<std::runtime_error>)
	{
		return false;
	}
}

struct PolishDiacritics : Diacritics
{
	constexpr PolishDiacritics (const char* name_, int charSize_,
		int a_, int c_, int e_, int l_, int n_, int o_, int s_, int x_, int z_,
		int A_, int C_, int E_, int L_, int N_, int O_, int S_, int X_, int Z_)
		: a(wchar_t(a_)), c(wchar_t(c_)), e(wchar_t(e_)), l(wchar_t(l_)), n(wchar_t(n_)), o(wchar_t(o_)), s(wchar_t(s_)), x(wchar_t(x_)), z(wchar_t(z_))
		, A(wchar_t(A_)), C(wchar_t(C_)), E(wchar_t(E_)), L(wchar_t(L_)), N(wchar_t(N_)), O(wchar_t(O_)), S(wchar_t(S_)), X(wchar_t(X_)), Z(wchar_t(Z_))
		, name(name_), charSize(charSize_)
	{
	}
	wchar_t a, c, e, l, n, o, s, x, z;
	wchar_t A, C, E, L, N, O, S, X, Z;
	const char* name;
	const int charSize;
	virtual wchar_t encode (char in) const override;
	virtual wchar_t decode (wchar_t in) const override;
	virtual inline int getSize () const override { return charSize; }
	virtual inline add_cptr<char> getName () const override { return name; }
};
wchar_t PolishDiacritics::encode (char in) const
{
	const wchar_t* map[]{	&a, &c, &e, &l, &n, &o, &s, &x, &z,
							&A, &C, &E, &L, &N, &O, &S, &X, &Z };
	const char* lts	=	"acelnosxz"
						"ACELNOSXZ";
	auto i = std::string(lts).find(in);
	return (std::string::npos != i) ? *map[i] : wchar_t(in);
}
wchar_t PolishDiacritics::decode (wchar_t in) const
{
	const wchar_t* map[]{	&a, &c, &e, &l, &n, &o, &s, &x, &z,
							&A, &C, &E, &L, &N, &O, &S, &X, &Z };
	const char* lts =	"acelnosxz"
						"ACELNOSXZ";
	auto i = std::find_if(std::begin(map), std::end(map),
		[&](add_cptr<wchar_t> c) { return *c == in; });
	return (std::end(map) != i) ? lts[std::distance(i, std::begin(map))] : in;
}

constexpr PolishDiacritics ISO_8859_2	{ "ISO_8859_2",	1,	0xB1, 0xE6, 0xEA, 0xB3, 0xF1, 0xF3, 0xB6, 0xBC, 0xBF, 0xA1, 0xC6, 0xCA, 0xA3, 0xD1, 0xD3, 0xA6, 0xAC, 0xAF };
constexpr PolishDiacritics CP_1250		{ "CP_1250",	1,	0xB9, 0xE6, 0xEA, 0xB3, 0xF1, 0xF3, 0x9C, 0x9F, 0xBF, 0xA5, 0xC6, 0xCA, 0xA3, 0xD1, 0xD3, 0x8C, 0x8F, 0xAF };
constexpr PolishDiacritics CP_852		{ "CP_852",		1,	0xA5, 0x86, 0xA9, 0x88, 0xE4, 0xA2, 0x98, 0xAB, 0xBE, 0xA4, 0x8F, 0xA8, 0x9D, 0xE3, 0xE0, 0x97, 0x8D, 0xBD };
constexpr PolishDiacritics Mazovia		{ "Mazovia",	1,	0x86, 0x8D, 0x91, 0x92, 0xA4, 0xA2, 0x9E, 0xA6, 0xA7, 0x8F, 0x95, 0x90, 0x9C, 0xA5, 0xA3, 0x98, 0xA0, 0xA1 };
constexpr PolishDiacritics Uni_UTF_8	{ "Uni_UTF_8",	2,	0xC485,	0xC487,	0xC499,	0xC582,	0xC584,	0xC3B3,	0xC59B,	0xC5BA,	0xC5BC,
															0xC484,	0xC486,	0xC498,	0xC581,	0xC583,	0xC393,	0xC59A,	0xC5B9,	0xC5BB };

//	constexpr PolishDiacritics CP_668		{ "CP_668",			1,	0xA5, 0x86, 0xA9, 0x88, 0xAD, 0xA2, 0x98, 0xAB, 0xA7, 0xA4, 0x8F, 0xA8, 0x9D, 0xAC, 0x9E, 0x97, 0x8D, 0xA6 };
//	constexpr PolishDiacritics AmigaPL		{ "AmigaPL",		1,	0xE2, 0xEA, 0xEB, 0xEE, 0xEF, 0xF3, 0xF4, 0xFA, 0xFB, 0xC2, 0xCA, 0xCB, 0xCE, 0xCF, 0xD3, 0xD4, 0xDA, 0xDB };
//	constexpr PolishDiacritics AtariCalamus	{ "AtariCalamus",	1,	0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9 };
//	constexpr PolishDiacritics ATM			{ "ATM",			1,	0xE4, 0xE7, 0xEB, 0xF0, 0xF1, 0xF3, 0xF6, 0xFA, 0xFC, 0xC4, 0xC7, 0xCB, 0xD0, 0xD1, 0xD3, 0xD6, 0xDA, 0xDC };
//	constexpr PolishDiacritics CorelDraw	{ "CorelDraw",		1,	0xE5, 0xEC, 0xE6, 0xC6, 0xF1, 0xF3, 0xA5, 0xAA, 0xBA, 0xC5, 0xF2, 0xC9, 0xA3, 0xD1, 0xD3, 0xFF, 0xE1, 0xED };
//	constexpr PolishDiacritics CSK			{ "CSK",			1,	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA8, 0xA7, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x88, 0x87 };
//	constexpr PolishDiacritics Cyfromat		{ "Cyfromat",		1,	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x98, 0x97, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x88, 0x87 };
//	constexpr PolishDiacritics DHN			{ "DHN",			1,	0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x91, 0x90, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x88, 0x87 };
//	constexpr PolishDiacritics ELWRO_Junior	{ "ELWRO_Junior",	1,	0xE1, 0xE3, 0xE5, 0xEC, 0xEE, 0xEF, 0xF3, 0xFA, 0xF9, 0xC1, 0xC3, 0xC5, 0xCC, 0xCE, 0xCF, 0xD3, 0xDA, 0xD9 };
//	constexpr PolishDiacritics IEA_Swierk	{ "IEA_Swierk",		1,	0xA0, 0x9B, 0x82, 0x9F, 0xA4, 0xA2, 0x87, 0xA8, 0x91, 0x8F, 0x80, 0x90, 0x9C, 0xA5, 0x99, 0xEB, 0x9D, 0x92 };
//	constexpr PolishDiacritics IINTE_ISIS	{ "IINTE_ISIS",		1,	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88 };
//	constexpr PolishDiacritics Logic		{ "Logic",			1,	0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88 };
//	constexpr PolishDiacritics Macintosh	{ "Macintosh",		1,	0x88, 0x8D, 0xAB, 0xB8, 0xC4, 0x97, 0xE6, 0x90, 0xFD, 0x84, 0x8C, 0xA2, 0xFC, 0xC1, 0xEE, 0xE5, 0x8F, 0xFB };
//	constexpr PolishDiacritics Mazovia_157	{ "Mazovia_157",	1,	0x86, 0x8D, 0x91, 0x92, 0xA4, 0xA2, 0x9D, 0xA6, 0xA7, 0x8F, 0x95, 0x90, 0x9C, 0xA5, 0xA3, 0x98, 0xA0, 0xA1 };
//	constexpr PolishDiacritics Mazovia_Fido	{ "Mazovia_Fido",	1,	0x86, 0x87, 0x91, 0x92, 0xA4, 0xA2, 0x9D, 0xA6, 0xA7, 0x8F, 0x80, 0x90, 0x9C, 0xA5, 0xA3, 0x98, 0xA0, 0xA1 };
//	constexpr PolishDiacritics Microvex		{ "Microvex",		1,	0xA0, 0x9B, 0x82, 0x9F, 0xA4, 0xA2, 0x87, 0xA8, 0x91, 0x8F, 0x80, 0x90, 0x9C, 0xA5, 0x93, 0x98, 0x9D, 0x92 };
//	constexpr PolishDiacritics TeXPL		{ "TexPL",			1,	0xA1, 0xA2, 0xA6, 0xAA, 0xAB, 0xF3, 0xB1, 0xB9, 0xBB, 0x81, 0x82, 0x86, 0x8A, 0x8B, 0xD3, 0x91, 0x99, 0x9B };
//	constexpr PolishDiacritics Unia			{ "Unia",			1,	0x86, 0x95, 0x82, 0x91, 0xA4, 0x94, 0x84, 0xA8, 0x81, 0x8F, 0x97, 0x90, 0x92, 0xA5, 0x99, 0x8E, 0xAD, 0x9A };
//	constexpr PolishDiacritics Ventura		{ "Ventura",		1,	0x96, 0x94, 0xA4, 0xA7, 0x91, 0xA2, 0x84, 0x82, 0x87, 0x97, 0x99, 0xA5, 0xA6, 0x92, 0x8F, 0x8E, 0x90, 0x80 };

std::vector<add_cptr<Diacritics>> getPolishDiacritics ()
{
	static const std::vector<PolishDiacritics> map
	{
		ISO_8859_2, CP_1250, CP_852, Mazovia, Uni_UTF_8,
		// AtariCalamus, ATM, CorelDraw, CP_668, CSK, AmigaPL,
		// Cyfromat, DHN, ELWRO_Junior, IEA_Swierk, IINTE_ISIS,
		// Logic, Macintosh, Mazovia_157, Mazovia_Fido, Microvex,
		// TeXPL, Unia, Ventura
	};
	std::vector<add_cptr<Diacritics>> res(map.size());
	std::transform(map.begin(), map.end(), res.begin(),
		[](add_cref<PolishDiacritics> d) { return &d; });
	return res;
}

std::vector	<add_cptr<Diacritics>> getDiacritics (add_cref<Locales> locales)
{
	switch (locales)
	{
		case Locales::pl_PL:	return getPolishDiacritics();
		default: break;
	}
	return {};
}

int findBestDiacritics (add_cref<std::vector<add_cptr<Diacritics>>>	diacritics,
						std::initializer_list<char>					encoded,
						std::initializer_list<wchar_t>				decoded)
{
	typedef std::initializer_list<char>::size_type size_type;
	const size_type N = std::min<size_type>(encoded.size(), decoded.size());
	if (N == 0) return (-1);
	const auto M = data_count(diacritics);
	std::map<uint32_t, uint32_t> scores;
	auto e = encoded.begin();
	auto d = decoded.begin();
	for (std::size_t i = 0; i < N; ++i)
	{
		for (uint32_t j = 0; j < M; ++j)
		{
			if (diacritics.at(j)->encode(*e) == *d)
				scores[j]++;
		}
		++e;
		++d;
	}
	uint32_t count = 0u;
	int best = (-1);
	for (add_cref<std::pair<const uint32_t, uint32_t>> score : scores)
	{
		if (score.second > count)
		{
			count = score.second;
			best = make_signed(score.first);
		}
	}
	return best;
}

std::string getAlternativeCodePage (add_cref<Locales> locales)
{
	UNREF(locales);

	std::string	name;
#if SYSTEM_OS_WINDOWS == 1
	const UINT acp = GetOEMCP();
	switch (acp)
	{
	case 852:	name = "CP_852";	break;	// OEM Latin 2; Central European (DOS)
	case 1250:	name = "CP_1250";	break;	// ANSI Central European; Central European (Windows)
	case 28592:	name = "CP_8859-2";	break;	// ISO 8859-2 Central European; Central European (ISO)
	case 1252:	// ANSI Latin 1; Western European (Windows)
	case 1257:	// Ansi Baltic, Baltic (Windows)
	case 28591:	// ISO 8859-1 Latin 1; Western European (ISO)
	default:	name = "???";
	}
#else
	name = "Uni_UTF_8";
#endif
	return name;
}

add_cptr<Diacritics> findDiacriticsByName (add_cref<Locales> locales, add_cref<std::string> codePage)
{
	const std::string name = codePage.empty() ? getAlternativeCodePage(locales) : codePage;
	std::vector<add_cptr<Diacritics>> v;
	switch (locales)
	{
	case Locales::pl_PL:	v = getPolishDiacritics();	break;
	default: break;
	}
	for (add_cptr<Diacritics> d : v)
	{
		if (name == d->getName())
			return d;
	}
	return nullptr;
}

add_ref<std::ostream> operator<< (add_ref<std::ostream> str, add_cref<WcharOrChar> c)
{
	if ((c.character & 0xFF) == c.character)
		return str << char(c.character & 0xFF);
	const char s[] { ((const char*)&c)[1], ((const char*)&c)[0], '\0', '\0' };
	return str << s;
}

void writeExampleText (std::ostream& str, const std::string& text, add_cref<Locales> locales, add_cptr<Diacritics> d)
{
	ASSERTION(d);
	const std::locale tmpLocale = makeLocale(locales);
	const std::locale curLocale = str.getloc();
	str.imbue(tmpLocale);
	for (const char c : text)
		str << d->encode(c);
	str.imbue(curLocale);
}

} // namespace vtf