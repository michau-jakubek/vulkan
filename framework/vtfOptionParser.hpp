#ifndef __VTF_OPTION_PARSER_HPP_INCLUDED__
#define __VTF_OPTION_PARSER_HPP_INCLUDED__

#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfVector.hpp"
#include "demangle.hpp"

#include <functional>
#include <optional>

namespace vtf
{

struct OptionParserState;

struct Option
{
	const char* name;
	uint32_t follows;
	constexpr Option (const char* name_, uint32_t follows_) : name(name_), follows(follows_) {}
	bool operator== (const Option& other) const;
};
int consumeOptions (add_cref<Option> opt, add_cref<std::vector<Option>> opts, add_ref<strings> args, add_ref<strings> values);
int consumeOptions (add_cref<Option> opt, add_cref<std::vector<add_cptr<Option>>> opts, add_ref<strings> args, add_ref<strings> values);

constexpr Option _optionShortHelp	("-h",		0);
constexpr Option _optionLongHelp	("--help",	0);

enum class OptionFlag : uint32_t
{
	None = 0b0,
	PrintDefault = 0b1
};
typedef Flags<uint32_t, OptionFlag> OptionFlags;

struct OptionInterface : public Option
{
	OptionInterface (add_cref<Option> opt)
		: Option		(opt)
		, m_desc		()
		, m_typeName	()
		, m_default		()
		, m_flags		()
		, m_parseState	(false)
		, m_touched		(false) {}
	virtual ~OptionInterface () = default;

	virtual	auto	getName			() const -> add_cref<std::string>	= 0;
	virtual auto	getType			() const -> std::type_index			= 0;
	virtual auto	getDTypeName	() const -> std::string             = 0; // demangle type name
	auto			getDesc			() const -> add_cref<std::string>	{ return m_desc; }
	void			setDesc			(add_cref<std::string> desc)		{ m_desc = desc; }
	auto			getFlags		() const -> add_cref<OptionFlags>	{ return m_flags; }
	void			setFlags		(add_cref<OptionFlags> flags)		{ m_flags = flags; }
	auto			getTypeName		() const -> add_cref<std::string>	{ return m_typeName; }
	void			setTypeName		(add_cref<std::string> typeName)	{ m_typeName = typeName; }
	auto			getDefault		() const -> add_cref<std::string>	{ return m_default; }
	void			setDefault		(add_cref<std::string> def)			{ m_default = def; }
	auto			getTouched		() const -> bool					{ return m_touched; }

	struct DefaultWriter			{ add_cref<OptionInterface> intf; };
	struct ValueWriter				{ add_cref<OptionInterface> intf; };
	auto			defaultWriter	() const -> DefaultWriter { return DefaultWriter{ *this }; }
	auto			valueWriter		() const -> ValueWriter { return ValueWriter{ *this }; }
	virtual auto	writeDefault	(add_ref<std::ostream> stream) const -> add_ref<std::ostream> = 0;
	virtual auto	writeValue		(add_ref<std::ostream> stream) const -> add_ref<std::ostream> = 0;
	virtual	bool	parse			(add_cref<std::string> text, add_ref<OptionParserState> state) = 0;
private:
	std::string	m_desc;
	std::string m_typeName;
	std::string m_default;
	OptionFlags	m_flags;
protected:
	bool		m_parseState;
	bool		m_touched;
};

inline add_ref<std::ostream> operator<< (add_ref<std::ostream> str, add_cref<OptionInterface::DefaultWriter> ow)
{
	return ow.intf.writeDefault(str);
}
inline add_ref<std::ostream> operator<< (add_ref<std::ostream> str, add_cref<OptionInterface::ValueWriter> ow)
{
	return ow.intf.writeValue(str);
}

typedef std::shared_ptr<OptionInterface> _OptIPtr;
typedef std::vector<_OptIPtr> _OptIPtrVec;

template<class X>
struct OptionT : public OptionInterface
{
	typedef std::function<X(add_cref<std::string>		text,
							add_cref<OptionT<X>>		sender,
							add_ref<bool>				status,
							add_ref<OptionParserState>	state)> parse_cb;
	typedef std::function<std::string(add_cref<OptionT<X>>	sender)> format_cb;										

	OptionT (add_ref<X> storage, add_cref<Option> opt, add_cref<std::string> desc, std::optional<X> def,
			OptionFlags flags, parse_cb parseCallback, format_cb formatCallback)
		: OptionInterface(opt)
		, m_storage			(storage)
		, m_name			(opt.name)
		, m_default			(def)
		, m_parseCallback	(parseCallback)
		, m_formatCallback	(formatCallback)
	{
		Option::name = m_name.c_str();
		setDesc(desc);
		setFlags(flags);
	}
	OptionT (add_ref<X> storage, add_cref<std::string> name, uint32_t follows, add_cref<std::string> desc, std::optional<X> def,
			OptionFlags flags, parse_cb parseCallback, format_cb formatCallback)
		: OptionInterface(Option(nullptr, follows))
		, m_storage			(storage)
		, m_name			(name)
		, m_default			(def)
		, m_parseCallback	(parseCallback)
		, m_formatCallback	(formatCallback)
	{
		Option::name = m_name.c_str();
		setDesc(desc);
		setFlags(flags);
	}
	virtual      ~OptionT		() = default;
	virtual bool parse			(add_cref<std::string> text, add_ref<OptionParserState> state) override;
	virtual auto getName		() const -> add_cref<std::string> override { return m_name; }
	virtual auto getType		() const -> std::type_index override { return std::type_index(typeid(X)); }
	virtual auto getDTypeName	() const -> std::string override {	return demangledName<X>(); }
	virtual auto writeDefault	(add_ref<std::ostream> str) const -> add_ref<std::ostream> override;
	virtual auto writeValue		(add_ref<std::ostream> str) const -> add_ref<std::ostream> override;
	add_ref<X>				m_storage;
	const std::string		m_name;
	std::optional<X>		m_default;
	parse_cb				m_parseCallback;
	format_cb				m_formatCallback;
};

struct OptionParserState
{
	bool				hasHelp;
	bool				hasErrors;
	bool				hasWarnings;
	std::ostringstream	messages;
	OptionParserState	();
	OptionParserState	(add_cref<OptionParserState>);
	auto				messagesText () const -> std::string;
	auto				operator= (add_cref<OptionParserState>) -> add_ref<OptionParserState>;
};

struct _OptionParserImpl
{
	_OptionParserImpl (bool includeHelp = true);
	_OptionParserImpl (_OptionParserImpl&& other) noexcept;
	auto	parse (add_cref<strings> cmdLineParams, bool allMustBeConsumed = true) -> strings;
	void	printOptions (add_ref<std::ostream> str, uint32_t descWidth = 40u, uint32_t indent = 2u) const;
	auto	getMaxOptionNameLength (bool includeTypeName = false, bool onlyTouched = false) const -> uint32_t;
	auto	getState() const -> add_cref<OptionParserState> { return m_state; }
	auto	getOptions () const -> _OptIPtrVec;
	auto	getOptionByName (add_cref<std::string> name) const ->_OptIPtr;

protected:
	void	addHelpOpts ();
	void	verifyExistingOptions () const;

	_OptIPtrVec			m_options;
	OptionParserState	m_state;
	const bool			m_includeHelp;
};

template<class UserParamsType>
struct OptionParser : public _OptionParserImpl
{
	OptionParser (OptionParser&& other);
	OptionParser (add_ref<UserParamsType> params, bool includeHelp = true);
	template<class X> _OptIPtr	addOption	(X UserParamsType::* storage, add_cref<Option> opt,
											add_cref<std::string> desc = {}, std::optional<X> defaultValue = {}, OptionFlags flags = {},
											typename OptionT<X>::parse_cb parseCallback = {}, typename OptionT<X>::format_cb formatCallback = {});
	template<class X> _OptIPtr	addOption	(X UserParamsType::* storage, add_cref<std::string> name, uint32_t follows,
											add_cref<std::string> desc = {}, std::optional<X> defaultValue = {}, OptionFlags flags = {},
											typename OptionT<X>::parse_cb parseCallback = {}, typename OptionT<X>::format_cb formatCallback = {});
private:
	add_ref<UserParamsType>		m_params;
};
template<class UserParamsType>
OptionParser<UserParamsType>::OptionParser (add_ref<UserParamsType> params, bool includeHelp)
	: _OptionParserImpl	(includeHelp)
	, m_params			(params)
{
}
template<class UserParamsType>
OptionParser<UserParamsType>::OptionParser (OptionParser&& other)
	: _OptionParserImpl	(std::move(other))
	, m_params			(other.m_params)
{
}
template<class UPT_> template<class X> inline _OptIPtr
OptionParser<UPT_>::addOption (X UPT_::* storage, add_cref<Option> opt, add_cref<std::string> desc, std::optional<X> defaultValue,
								OptionFlags flags, typename OptionT<X>::parse_cb parseCallback, typename OptionT<X>::format_cb formatCallback)
{
	auto option = std::make_shared<OptionT<X>>(m_params.*storage, opt, desc, defaultValue, flags, parseCallback, formatCallback);
	m_options.push_back(option);
	verifyExistingOptions();
	return option;
}
template<class UPT_> template<class X> inline _OptIPtr
OptionParser<UPT_>::addOption (X UPT_::* storage, add_cref<std::string> name, uint32_t follows, add_cref<std::string> desc,
								std::optional<X> defaultValue, OptionFlags flags, typename OptionT<X>::parse_cb parseCallback,
								typename OptionT<X>::format_cb formatCallback)
{
	auto option = std::make_shared<OptionT<X>>(m_params.*storage, name, follows, desc, defaultValue, flags, parseCallback, formatCallback);
	m_options.push_back(option);
	verifyExistingOptions();
	return option;
}

template<class X>
struct FromTextDispatcher
{
	inline static X fromText (add_cref<std::string> text, add_cref<X> def, add_ref<bool> status)
	{
		return ::vtf::fromText(text, def, status);
	}
};
template<class T, size_t N>
struct FromTextDispatcher<VecX<T, N>>
{
	inline static VecX<T, N> fromText (add_cref<std::string> text, add_cref<VecX<T, N>> def, add_ref<bool> status)
	{
		std::array<bool, N> states;
		return VecX<T, N>::fromText(text, def, states, &status);
	}
};

template<class X>
add_ref<std::ostream> OptionT<X>::writeDefault (add_ref<std::ostream> stream) const
{
	if (getDefault().empty())
		return stream << ((m_default.has_value()) ? *m_default : m_storage);
	return stream << getDefault();
}

template<class X>
add_ref<std::ostream> OptionT<X>::writeValue (add_ref<std::ostream> stream) const
{
	if (m_formatCallback)
		return stream << m_formatCallback(*this);
	if constexpr (std::is_same_v<X, bool>)
		return stream << std::boolalpha << m_storage << std::noboolalpha;
	else
		return stream << m_storage;
}

template<class X>
bool OptionT<X>::parse (add_cref<std::string> text, add_ref<OptionParserState> state)
{
	m_touched = true;
	const X result = m_parseCallback
		? m_parseCallback(text, *this, m_parseState, state)
		: FromTextDispatcher<X>::fromText(text, (m_default.has_value() ? *m_default : m_storage), m_parseState);
	if (m_parseState)
		m_storage = result;
	else if (!m_parseCallback)
	{
		state.hasWarnings = true;
		state.messages	<< "WARNING: Unable to parse " << std::quoted(m_name) << " from " << std::quoted(text)
						<< ", default value " << defaultWriter() << " was used" << std::endl;
	}
	return m_parseState;
}

} // namespace vtf

#endif // __VTF_OPTION_PARSER_HPP_INCLUDED__
