#include "vtfOptionParser.hpp"
#include <iterator>

namespace vtf
{

bool Option::operator== (const Option& other) const
{
	if (name == nullptr && name == other.name)
		return other.follows == follows;
	return std::string(name).compare(other.name) == 0 && other.follows == follows;
}

static int consumeOptions (add_cref<Option>							opt,
						   add_cref<std::vector<add_cptr<Option>>>	opts,
						   uint32_t									arg,
						   add_ref<strings>							args,
						   add_ref<strings>							values,
						   add_ref<bool>							error)
{
	if (arg >= args.size()) return 0;

	uint32_t	skip = 0;
	int			found = 0;

	if (std::strcmp(opt.name, args[arg].c_str()) == 0)
	{
		for (uint32_t follows = 0; follows < opt.follows; ++follows)
		{
			if (arg + 1 < args.size())
			{
				values.push_back(args[arg + 1]);
				args.erase(std::next(args.begin(), arg + 1));
			}
			else
			{
				error = true;
				break;
			}
		}
		args.erase(std::next(args.begin(), arg));
		found = error ? 0 : 1;
	}

	if (!error && found == 0)
	{
		skip = 1;
		for (add_cptr<Option> o : opts)
		{
			if (std::strcmp(args[arg].c_str(), o->name) == 0)
			{
				skip += o->follows;
				break;
			}
		}
	}

	return error ? 0 : (found + consumeOptions(opt, opts, arg + skip, args, values, error));
}

int consumeOptions (add_cref<Option> opt, add_cref<std::vector<add_cptr<Option>>> opts, add_ref<strings> args, add_ref<strings> values)
{
	bool error = false;
	int count = consumeOptions(opt, opts, 0, args, values, error);
	return error ? (-1) : count;
}

int consumeOptions (add_cref<Option> opt, add_cref<std::vector<Option>> opts, add_ref<strings> args, add_ref<strings> values)
{
	std::vector<add_cptr<Option>> options(opts.size());
	std::transform(opts.begin(), opts.end(), options.begin(), [](add_cref<Option> opt){ return &opt; });
	return consumeOptions(opt, options, args, values);
}

OptionParserState::OptionParserState ()
	: hasHelp(), hasErrors(), hasWarnings(), messages()
{
}

OptionParserState::OptionParserState(OptionParserState&& src)
	: hasHelp(src.hasHelp)
	, hasErrors(src.hasErrors)
	, hasWarnings(src.hasWarnings)
	, messages(src.messagesText())
{
}

OptionParserState::OptionParserState (add_cref<OptionParserState> src)
	: hasHelp		(src.hasHelp)
	, hasErrors		(src.hasErrors)
	, hasWarnings	(src.hasWarnings)
	, messages		(src.messagesText())
{
}

add_ref<OptionParserState> OptionParserState::operator= (add_cref<OptionParserState> src)
{
	hasHelp		= (src.hasHelp);
	hasErrors	= (src.hasErrors);
	hasWarnings = (src.hasWarnings);
	messages.str(src.messagesText());
	return *this;
}

std::string OptionParserState::messagesText () const
{
	return messages.str();
}

_OptionParserImpl::_OptionParserImpl (std::unique_ptr<OptionParserState> state, bool includeHelp)
	: m_options		()
	, m_includeHelp	(includeHelp)
	, m_state		(std::move(state))
{
	if (includeHelp) addHelpOpts();
}

_OptionParserImpl::_OptionParserImpl (_OptionParserImpl&& other) noexcept
	: m_options		(other.m_options)
	, m_includeHelp	(other.m_includeHelp)
	, m_state		(std::move(other.m_state))
{
	if (m_includeHelp)
	{
		auto match = [](_OptIPtr ptr, add_cref<Option> opt) { return *ptr == opt; };
		m_options.erase(std::find_if(m_options.begin(), m_options.end(), std::bind(match, std::placeholders::_1, _optionShortHelp)));
		m_options.erase(std::find_if(m_options.begin(), m_options.end(), std::bind(match, std::placeholders::_1, _optionLongHelp)));
		addHelpOpts();
	}
}

void _OptionParserImpl::addHelpOpts ()
{
	const std::string comment("print this help");
	m_options.insert(m_options.begin(), std::make_shared<OptionT<bool>>(m_state->hasHelp, _optionLongHelp,
		comment, false, OptionFlags(), typename OptionT<bool>::parse_cb(), typename OptionT<bool>::format_cb()));
	m_options.insert(m_options.begin(), std::make_shared<OptionT<bool>>(m_state->hasHelp, _optionShortHelp,
		comment, false, OptionFlags(), typename OptionT<bool>::parse_cb(), typename OptionT<bool>::format_cb()));
}

strings _OptionParserImpl::parse (add_cref<strings> cmdLineParams, bool allMustBeConsumed, parse_cb onParsing)
{
	strings sink, args(cmdLineParams);
	const std::string truestring("true");
	std::vector<add_cptr<Option>> opts(m_options.size());
	std::transform(m_options.begin(), m_options.end(), opts.begin(), [](add_cref<std::shared_ptr<Option>> ptr) { return ptr.get(); });
	for (add_ref<_OptIPtr> opt : m_options)
	{
		if (consumeOptions(*opt, opts, args, sink) > 0)
		{
			if (false == (onParsing && onParsing(opt, (opt->follows == 0) ? truestring : sink.back(), m_state.get())))
			{
				opt->parse((opt->follows == 0) ? truestring : sink.back(), *m_state);
			}
		}
	}
	if (allMustBeConsumed && !args.empty())
	{
		m_state->messages << "ERROR: Unknown parameter " << std::quoted(args.front()) << std::endl;
		m_state->hasErrors = true;
	}

	return args;
}

void _OptionParserImpl::verifyExistingOptions () const
{
	for (auto i = m_options.begin(); i != m_options.end(); ++i)
	{
		for (auto j = std::next(i); j != m_options.end(); ++j)
		{
			add_cref<std::string> iname = (*i)->getName();
			ASSERTMSG(iname != (*j)->getName(), "Doubled option " + iname);
		}
	}
}

_OptIPtrVec _OptionParserImpl::getOptions () const
{
	_OptIPtrVec result;
	for (add_cref<_OptIPtr> opt : m_options)
	{
		if (!(*opt == _optionShortHelp || *opt == _optionLongHelp))
			result.emplace_back(opt);
	}
	return result;
}

_OptIPtr _OptionParserImpl::getOptionByName (add_cref<std::string> name) const
{
	auto match = [&](_OptIPtr ptr) { return ptr->getName() == name; };
	auto ptr = std::find_if(m_options.begin(), m_options.end(), match);
	return (ptr != m_options.end()) ? *ptr : _OptIPtr();
}

uint32_t _OptionParserImpl::getMaxOptionNameLength (bool includeTypeName, bool onlyTouched) const
{
	uint32_t maxOptNameLength = 0;

	for (add_cref<_OptIPtr> opt : m_options)
	{
		if (const bool process = onlyTouched ? opt->getTouched() : true; !process)
		{
			continue;
		}

		uint32_t typeNameLength = 0u;
		const uint32_t optNameLength = uint32_t(opt->getName().length());
		if (includeTypeName)
		{
			const std::string typeName = opt->getTypeName().empty() ? opt->getDTypeName() : opt->getTypeName();
			typeNameLength = (opt->follows == 0) ? 0u : (uint32_t(typeName.length()) + 3u);
		}
		maxOptNameLength = std::max(maxOptNameLength, (optNameLength + typeNameLength));
	}

	return maxOptNameLength;
}

static void OptionParserImplPutDescrition (_OptIPtr opt, add_ref<std::ostream> str, uint32_t descWidth, uint32_t maxOptNameLength, uint32_t indent)
{
	auto wordize = [&](add_cref<std::string> suffix = {}) -> strings
	{
		strings words;
		std::istringstream iss(opt->getDesc() + suffix);
		while (iss.tellg() >= 0)
		{
			std::string s;
			iss >> s;
			words.emplace_back(std::move(s));
		}
		return words;
	};

	auto search = [&](auto first, auto last, add_cref<std::string> sep)
	{
		uint32_t n = 0;
		auto j = first;

		while (j != last && (n + uint32_t(j->length())) < descWidth)
		{
			if (std::distance(first, j))
			{
				n += uint32_t(sep.length());
			}
			n += uint32_t(j->length());
			++j;
		}
		return j;
	};

	auto makeDefaultString = [&]() -> std::string
	{
		std::ostringstream oss;
		oss << ", default is " << opt->defaultWriter();
		return oss.str();
	};

	const std::string	def		= (opt->getFlags().contain(OptionFlag::PrintDefault) ? makeDefaultString() : std::string());
	const strings		desc	= wordize(def);
	const std::string	sep		(" ");
	const std::string	sindent	(maxOptNameLength + 10 + indent, ' ');
	bool				skip	= true;

	auto i = desc.cbegin();
	auto j = search(i, desc.cend(), sep);
	while (i != j)
	{
		if (skip)
			skip = false;
		else
		{
			str << sindent;
		}

		std::copy(i, j, std::ostream_iterator<std::string>(str, sep.c_str()));
		str << std::endl;

		i = j;
		j = search(i, desc.cend(), sep);
	}

}

void _OptionParserImpl::printOptions (add_ref<std::ostream> str, uint32_t descWidth, uint32_t indent) const
{
	const uint32_t		maxOptNameLength	= getMaxOptionNameLength(true);
	const std::string	strIndent			(indent, ' ');

	for (add_cref<_OptIPtr> opt : m_options)
	{
		str << strIndent << opt->getName() << ' ';
		auto alreadyWriten = opt->getName().length() + indent + 1u;;

		if (opt->follows == 0)
		{
			str << std::string((maxOptNameLength + 10 - alreadyWriten), ' ');
		}
		else
		{
			const std::string typeName = opt->getTypeName().empty() ? opt->getDTypeName() : opt->getTypeName();
			alreadyWriten += typeName.length() + 2;
			str << '<' << typeName << '>' << std::string((maxOptNameLength + 10 - alreadyWriten), ' ');
		}

		OptionParserImplPutDescrition(opt, str, descWidth, maxOptNameLength, indent);
	}
}

} // namespace vtf
