#include "vtfCommandLine.hpp"

#include <iterator>
#include <numeric>
#include <string_view>

namespace vtf
{

bool Option::operator== (const Option& other) const
{
	return std::string_view(name) == std::string_view(other.name) && other.follows == follows;
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
		comment, false, OptionFlags(OptionFlag::DontPrintAsParams), typename OptionT<bool>::parse_cb(), typename OptionT<bool>::format_cb()));
	m_options.insert(m_options.begin(), std::make_shared<OptionT<bool>>(m_state->hasHelp, _optionShortHelp,
		comment, false, OptionFlags(OptionFlag::DontPrintAsParams), typename OptionT<bool>::parse_cb(), typename OptionT<bool>::format_cb()));
}

strings _OptionParserImpl::parse (add_cref<strings> cmdLineParams, bool allMustBeConsumed, parse_cb onParsing)
{
	strings sink, args(cmdLineParams);
	const std::string truestring("true");
	std::vector<add_cptr<Option>> opts(m_options.size());
	std::transform(m_options.begin(), m_options.end(), opts.begin(),
					[](add_cref<std::shared_ptr<Option>> ptr) { return ptr.get(); });
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

strings _OptionParserImpl::parse (add_ref<CommandLine> commandLine, bool allMustBeConsumed, parse_cb onParsing)
{
	strings sink;
	const std::string truestring("true");
	std::vector<Option> opts;
	opts.reserve(m_options.size());
	std::transform(m_options.begin(), m_options.end(), std::back_inserter(opts),
					[](add_cref<std::shared_ptr<Option>> ptr) { return *ptr; });
	for (add_ref<_OptIPtr> opt : m_options)
	{
		const int c = commandLine.consumeOptions(*opt, opts, sink);
		if (c > 0)
		{
			if (false == (onParsing && onParsing(opt, (opt->follows == 0) ? truestring : sink.back(), m_state.get())))
			{
				opt->parse((opt->follows == 0) ? truestring : sink.back(), *m_state);
			}
		}
		else if (c < 0)
		{
			m_state->messages << "ERROR: \"" << opt->getName() << "\" must have a value";
			m_state->hasErrors = true;
		}
	}
	const strings unconsumed = commandLine.getUnconsumedTokens();
	if (allMustBeConsumed && unconsumed.size())
	{
		m_state->messages << "ERROR: Unknown parameter " << std::quoted(unconsumed[0]) << std::endl;
		m_state->hasErrors = true;
	}

	return unconsumed;
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

_OptIPtr _OptionParserImpl::getOptionByName (add_cref<Option> option) const
{
	const std::string name = option.name;
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
		oss << ", default is ";
		if (opt->getFlags().contain(OptionFlag::PrintValueAsDefault))
			oss << opt->valueWriter();
		else oss << opt->defaultWriter();
		return oss.str();
	};

	const std::string	def		= (opt->getFlags().contain(OptionFlag::PrintDefault)
								  || opt->getFlags().contain(OptionFlag::PrintValueAsDefault)) ? makeDefaultString() : std::string();
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

void _OptionParserImpl::printParams (add_cref<std::string> header, add_ref<std::ostream> str, bool twoNewLines) const
{
	uint32_t maxOptionNameLength = 0u;
	for (add_cref<_OptIPtr> opt : m_options)
	{
		if (false == opt->getFlags().contain(OptionFlag::DontPrintAsParams))
			maxOptionNameLength = std::max(maxOptionNameLength, uint32_t(opt->getParamName().length()));
	}
	if (const auto hdrLength = header.length(); hdrLength)
	{
		str << header << std::endl << std::string(hdrLength, '-') << std::endl;
	}
	if (maxOptionNameLength)
	{
		for (add_cref<_OptIPtr> opt : m_options)
		{
			if (false == opt->getFlags().contain(OptionFlag::DontPrintAsParams))
			{
				str << opt->getParamName() << ':'
					<< std::string((maxOptionNameLength - uint32_t(opt->getParamName().length()) + 2u), ' ')
					<< opt->valueWriter() << std::endl;
				if (twoNewLines) str << std::endl;
			}
		}
	}
	else
	{
		for (add_cref<_OptIPtr> opt : m_options)
			if (false == opt->getFlags().contain(OptionFlag::DontPrintAsParams))
			{
				str << opt->valueWriter() << std::endl;
				if (twoNewLines) str << std::endl;
			}
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

CommandLine::CommandLine (int argc, char** argv, add_cref<std::string> userData)
	: m_anchors()
	, m_commandLine()
	, m_appName()
	, m_userData(userData)
{
	makeAnchors(argc, argv);
}

CommandLine::CommandLine (add_cref<strings> input, add_cref<std::string> userData)
	: m_anchors()
	, m_commandLine()
	, m_appName()
	, m_userData(userData)
{
	const uint32_t argc = data_count(input);
	std::vector<add_ptr<char>> argv(argc);

	for (uint32_t arg = 0u; arg < argc; ++arg)
		argv[arg] = const_cast<add_ptr<char>>(input[arg].c_str());

	makeAnchors(make_signed(argc), argv.data());
}

CommandLine::CommandLine (add_cref<std::string> multiString, add_cref<std::string> userData)
	: CommandLine(parseMultiString(multiString), userData)
{
}

void CommandLine::makeAnchors (int argc, char** argv)
{
	m_anchors.resize(make_unsigned(argc - 1));

	m_appName.assign(argv[0]);
	std::vector<uint32_t> lengths(m_anchors.size());

	for (uint32_t i = 0; i < make_unsigned(argc); ++i)
	{
		if (0u == i) continue; // skipp program name

		lengths[i - 1] = uint32_t(std::strlen(argv[i]));
		m_commandLine.append(argv[i]);
	}

	uint32_t offset = 0;
	const std::string_view cmdLine(m_commandLine);

	for (uint32_t i = 0; i < make_unsigned(argc); ++i)
	{
		if (0u == i) continue; // skipp program name

		m_anchors[i - 1].view = cmdLine.substr(offset, lengths[i - 1]);
		m_anchors[i - 1].consumed = false;

		offset += lengths[i - 1];
	}

	ASSERTION(m_commandLine.length() == offset);
}

int CommandLine::consumeOptions (
	add_cref<Option> opt, add_cref<std::vector<Option>> opts,
	add_ref<strings> values, add_cref<strings> breakValues)
{
	int  depth = 0;
	const auto count = int(m_anchors.size());
	return consumeOptions(0, count, opt, opts, values, breakValues, depth);
}

uint32_t CommandLine::getConsumedTokenCount () const
{
	return uint32_t(std::count_if(m_anchors.begin(), m_anchors.end(),
								  [](add_cref<Anchor> a) { return a.consumed; }));
}

strings CommandLine::getUnconsumedTokens() const
{
	strings unconsumed(m_anchors.size() - getConsumedTokenCount());
	int i = 0;
	for (add_cref<Anchor> a : m_anchors)
		if (a.consumed == false)
			unconsumed[i++] = a.view;
	return unconsumed;
}

uint32_t CommandLine::truncateConsumed ()
{
	const auto oldSize = m_anchors.size();
	auto isConsumed = [](add_cref<Anchor> a) { return a.consumed; };
	m_anchors.erase(std::remove_if(m_anchors.begin(), m_anchors.end(), isConsumed), m_anchors.end());
	return uint32_t(oldSize - m_anchors.size());
}

strings CommandLine::getStrings () const
{
	const auto c = data_count(m_anchors);
	strings ss(1u + c);
	ss[0] = m_appName;
	for (uint32_t i = 0u; i < c; ++i)
		ss[i + 1u].assign(m_anchors[i].view.data(), m_anchors[i].view.size());
	return ss;
}

std::string CommandLine::getMultiString () const
{
	std::string result;

	// count needed size in advance (optimization)
	size_t total = (m_appName.length() + 1u) + 1u; // ending \0
	for (add_cref<Anchor> a : m_anchors)
		total += a.view.size() + 1u;

	result.reserve(total);

	result.append(m_appName.data(), m_appName.length());
	result.push_back('\0');

	// build final multi-string
	for (add_cref<Anchor> a : m_anchors) {
		result.append(a.view.data(), a.view.size());
		result.push_back('\0');
	}

	result.push_back('\0'); // ending \0\0

	return result;
}

std::string CommandLine::getParams () const
{
	int i = 0;
	std::ostringstream os;
	for (add_cref<Anchor> a : m_anchors)
	{
		if (i++) os << ' ';
		os << std::string(a.view.data(), a.view.size());
	}
	return os.str();
}

add_cref<std::string> CommandLine::getAppName() const { return m_appName; }
add_cref<std::string> CommandLine::getUserData () const { return m_userData; }

int CommandLine::consumeOptions (
	const int first, const int count,
	add_cref<Option> opt, add_cref<std::vector<Option>> opts, add_ref<strings> values,
	add_cref<strings> breakValues, add_ref<int> depth)
{
	depth += 1;
	auto isBreakValue = [&](add_cref<Anchor> a) -> bool {
		for (add_cref<std::string> bv : breakValues)
			if (a.view == std::string_view(bv))
				return true;
		return false;
	};
	auto findOption = [&](add_cref<Anchor> a, add_ref<Option> x) -> bool
	{
		auto ptr = std::find_if(opts.begin(), opts.end(),
			[&](add_cref<Option> y) { return a.view == std::string_view(y.name); });
		if (ptr != opts.end()) {
			x = *ptr;
			return true;
		}
		return false;
	};

	for (int i = first; i < count; ++i)
	{
		add_ref<Anchor> a = m_anchors[i];
		if (a.consumed) continue;

		if (isBreakValue(a)) {
			break;
		}

		Option o(opt);

		if (findOption(a, o))
		{
			if (opt != o) {
				i += (o.follows ? 1 : 0);
				continue;
			}

			if (o.follows == 0u) {
				a.consumed = true;
				return 1u;
			}
			else {
				a.consumed = true;
				if (const int k = i + 1; k < count) {
					m_anchors[k].consumed = true;
					values.emplace_back(m_anchors[k].view.data(), m_anchors[k].view.size());
					if (make_unsigned(depth) < o.follows) {
						const int n = consumeOptions(k + 1, count, opt, opts, values, breakValues, depth);
						if (n < 0)
							return (-1);
						else if (n == 0)
							return depth;
						else
							return n;
					}
					else {
						return depth;
					}
				}
				else {
					return (-1);
				}
			}
		}
	}

	depth -= 1;
	return 0;
}


} // namespace vtf
