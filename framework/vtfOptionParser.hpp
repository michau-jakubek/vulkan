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
	uint32_t id;
	constexpr Option (const char* name_, uint32_t follows_, uint32_t id_ = 0u)
		: name(name_), follows(follows_), id(id_) {}
	bool operator== (const Option& other) const;
};
int consumeOptions (add_cref<Option> opt, add_cref<std::vector<Option>> opts, add_ref<strings> args, add_ref<strings> values);
int consumeOptions (add_cref<Option> opt, add_cref<std::vector<add_cptr<Option>>> opts, add_ref<strings> args, add_ref<strings> values);

constexpr Option _optionShortHelp	("-h",		0);
constexpr Option _optionLongHelp	("--help",	0);

enum class OptionFlag : uint32_t
{
	None				= 0b0,
	PrintDefault		= 0b1,
	PrintValueAsDefault	= 0b10,
	DontPrintAsParams	= 0b100,
};
typedef Flags<uint32_t, OptionFlag> OptionFlags;

struct OptionInterface : public Option
{
	OptionInterface (add_cref<Option> opt)
		: Option		(opt)
		, m_desc		()
		, m_typeName	()
		, m_default		()
		, m_paramName	()
		, m_flags		()
		, m_parseState	(false)
		, m_touched		(false) {}
	virtual ~OptionInterface () = default;
	using mptr = add_ptr<OptionInterface>;

	virtual	auto	getName			() const -> add_cref<std::string>	= 0;
	virtual auto	getType			() const -> std::type_index			= 0;
	virtual auto	getDTypeName	() const -> std::string             = 0; // demangle type name
	auto			getDesc			() const -> add_cref<std::string>	{ return m_desc; }
	mptr			setDesc			(add_cref<std::string> desc)		{ m_desc = desc; return this;}
	auto			getFlags		() const -> add_cref<OptionFlags>	{ return m_flags; }
	mptr			setFlags		(add_cref<OptionFlags> flags)		{ m_flags = flags; return this;}
	auto			getTypeName		() const -> add_cref<std::string>	{ return m_typeName; }
	mptr			setTypeName		(add_cref<std::string> typeName)	{ m_typeName = typeName; return this;}
	auto			getDefault		() const -> add_cref<std::string>	{ return m_default; }
	mptr			setDefault		(add_cref<std::string> def)			{ m_default = def; return this;}
	auto			getTouched		() const -> bool					{ return m_touched; }
	auto			getParamName	() const -> add_cref<std::string>	{ return m_paramName; }
	mptr			setParamName	(add_cref<std::string> paramName)	{ m_paramName = paramName; return this; }

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
	std::string m_paramName;
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

#define DEF_OPTION_TYPE_NAME(type__, name__) \
	template<> struct OptionTypeName<type__> { \
		std::string get () const { return name__; } }
template<class X> struct OptionTypeName
{
	std::string get () const { return demangledName<X>(); }
};
DEF_OPTION_TYPE_NAME(std::string, "text");
DEF_OPTION_TYPE_NAME(int32_t, "int");
DEF_OPTION_TYPE_NAME(uint32_t, "uint");
DEF_OPTION_TYPE_NAME(float, "float");
DEF_OPTION_TYPE_NAME(double, "double");
template<class X, size_t N> struct OptionTypeName<VecX<X, N>>
{
	std::string get () const { return (OptionTypeName<X>().get().substr(0, 1) + "vec" + std::to_string(N)); }
};

typedef std::shared_ptr<OptionInterface> _OptIPtr;
typedef std::vector<_OptIPtr> _OptIPtrVec;

template<class X>
struct OptionT : public OptionInterface
{
	typedef std::function<X(add_cref<std::string>		text,
							add_cref<OptionT<X>>		sender,
							add_ref<bool>				status,
							add_ref<OptionParserState>	state)> parse_cb;
	typedef std::function<std::string(add_cref<OptionT<X>> sender)> format_cb;

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
	virtual auto getDTypeName	() const -> std::string override {	return OptionTypeName<X>().get(); }
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
	OptionParserState	(OptionParserState&&);
	OptionParserState	(add_cref<OptionParserState>);
	auto				messagesText () const -> std::string;
	auto				operator= (add_cref<OptionParserState>) -> add_ref<OptionParserState>;
};

struct _OptionParserImpl
{
	typedef std::function<bool(std::shared_ptr<OptionInterface> option, add_cref<std::string> value, add_ptr<OptionParserState> state)> parse_cb;

	_OptionParserImpl (std::unique_ptr<OptionParserState>, bool includeHelp = true);
	_OptionParserImpl (_OptionParserImpl&& other) noexcept;
	auto	parse (add_cref<strings> cmdLineParams, bool allMustBeConsumed = true, parse_cb = {}) -> strings;
	void	printOptions (add_ref<std::ostream> str, uint32_t descWidth = 40u, uint32_t indent = 2u) const;
	auto	getMaxOptionNameLength (bool includeTypeName = false, bool onlyTouched = false) const -> uint32_t;
	auto	getOptions () const -> _OptIPtrVec;
	auto	getOptionByName (add_cref<Option> option) const ->_OptIPtr;
	void	printParams (add_cref<std::string> header, add_ref<std::ostream> str, bool twoNewLines = false) const;

protected:
	void	addHelpOpts ();
	void	verifyExistingOptions () const;

	_OptIPtrVec							m_options;
	const bool							m_includeHelp;
	std::unique_ptr<OptionParserState>	m_state;
};

template<class UserParamsType, class OptionParserStateType = OptionParserState,
	class = std::enable_if_t<std::is_constructible_v<OptionParserState*, OptionParserStateType*>>>
struct OptionParser : public _OptionParserImpl
{
	typedef std::function<bool(add_ref<UserParamsType> parser,
		std::shared_ptr<OptionInterface> option, add_cref<std::string> value, add_ref<OptionParserStateType> state)> parse_cb;

	OptionParser (OptionParser&& other) noexcept;
	OptionParser (add_ref<UserParamsType> params, bool includeHelp = true);
	template<class X> _OptIPtr	addOption	(X UserParamsType::* storage, add_cref<Option> opt,
											add_cref<std::string> desc = {}, std::optional<X> defaultValue = {}, OptionFlags flags = {},
											typename OptionT<X>::parse_cb parseCallback = {}, typename OptionT<X>::format_cb formatCallback = {});
	template<class X> _OptIPtr	addOption	(X UserParamsType::* storage, add_cref<std::string> name, uint32_t follows,
											add_cref<std::string> desc = {}, std::optional<X> defaultValue = {}, OptionFlags flags = {},
											typename OptionT<X>::parse_cb parseCallback = {}, typename OptionT<X>::format_cb formatCallback = {});
	auto						getState	() const -> add_cref<OptionParserStateType>;
private:
	add_ref<UserParamsType>		m_params;
};
template<class UserParamsType, class OptionParserStateType, class Enable>
OptionParser<UserParamsType, OptionParserStateType, Enable>::OptionParser (add_ref<UserParamsType> params, bool includeHelp)
	: _OptionParserImpl	(std::make_unique<OptionParserStateType>(), includeHelp)
	, m_params			(params)
{
}
template<class UserParamsType, class OptionParserStateType, class Enable>
OptionParser<UserParamsType, OptionParserStateType, Enable>::OptionParser (OptionParser&& other) noexcept
	: _OptionParserImpl	(std::move(other))
	, m_params			(other.m_params)
{
}
template<class UPT_, class OPST_, class E_> template<class X> inline _OptIPtr
OptionParser<UPT_, OPST_, E_>::addOption (X UPT_::* storage, add_cref<Option> opt, add_cref<std::string> desc, std::optional<X> defaultValue,
								OptionFlags flags, typename OptionT<X>::parse_cb parseCallback, typename OptionT<X>::format_cb formatCallback)
{
	auto option = std::make_shared<OptionT<X>>(m_params.*storage, opt, desc, defaultValue, flags, parseCallback, formatCallback);
	m_options.push_back(option);
	verifyExistingOptions();
	return option;
}
template<class UPT_, class OPST_, class E_> template<class X> inline _OptIPtr
OptionParser<UPT_, OPST_, E_>::addOption (X UPT_::* storage, add_cref<std::string> name, uint32_t follows, add_cref<std::string> desc,
								std::optional<X> defaultValue, OptionFlags flags, typename OptionT<X>::parse_cb parseCallback,
								typename OptionT<X>::format_cb formatCallback)
{
	auto option = std::make_shared<OptionT<X>>(m_params.*storage, name, follows, desc, defaultValue, flags, parseCallback, formatCallback);
	m_options.push_back(option);
	verifyExistingOptions();
	return option;
}
template<class UPT_, class OPST_, class E_> inline
add_cref<OPST_> OptionParser<UPT_, OPST_, E_>::getState () const
{
	return *reinterpret_cast<add_cptr<OPST_>>(m_state.get());
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
