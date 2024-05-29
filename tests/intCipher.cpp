#include "intCipher.hpp"
#include "vtfOptionParser.hpp"
#include "vtfBacktrace.hpp"

#include <iomanip>
#if SYSTEM_OS_LINUX == 1
 #include <termios.h>	// tc(g|s)etattr
#else
 #include <Windows.h>
#endif

#ifndef __CHAR_BIT__
#define __CHAR_BIT__ 8
#endif

namespace
{
using namespace vtf;
typedef add_ref<std::ostream>		ostream_ref;
typedef add_ref<std::string>		string_ref;
typedef add_cref<std::string>		cstring_ref;

struct Params
{
	enum class Action : uintptr_t
	{
		None, Encode, Decode,
	}			action;
	enum class Source
	{
		None, File, Phrase
	}			source;
	enum class Method
	{
		None, Xor
	}			method;
	struct {
		uint32_t hex_input	: 1;
		uint32_t hex_output	: 1;
	}
	flags { 0, 0 };
	std::string	cipher;
	std::string	input;
	std::string	output;
	Params ();
	void print (ostream_ref log) const;
	static void usage (cstring_ref appName, add_cptr<char> testName, ostream_ref log);
	static std::string readPhrase (ostream_ref log, add_ref<std::stringstream> errColl);
	static auto parseCommandLine (add_cref<strings> cmdLineParams, ostream_ref log) -> std::tuple<bool, Params, std::string>;
};
Params::Params ()
	: action	(Action::None)
	, source	(Source::None)
	, method	(Method::Xor)
	, cipher	()
	, input		()
	, output	()
{
}
void Params::print (ostream_ref log) const
{
	log << "Action: " << (int)action << std::endl
		<< "Source: " << (int)source << std::endl
		<< "Cipher: " << ((source == Source::Phrase) ? "********" : cipher) << std::endl
		<< "Input" << ((flags.hex_input != 0) ? "(hex): " : ": ") << input << std::endl
		<< "Output" << ((flags.hex_output != 0) ? "(hex): " : ": ") << output << std::endl;
}
void Params::usage (cstring_ref appName, add_cptr<char> testName, ostream_ref log)
{
	log << "Usage:\n"
		<< "   --encode                 default if not specified\n"
		<< "              or\n"
		   "   --decode                 must be specified if --encode is not\n\n"
		<< "   -phrase                  cipher by phrase, default, you will be ask for it\n"
		<< "              or\n"
		<< "   -file <file>             cipher by exisiting file\n"
		<< "                            must be different from the output file name\n\n"
		<< "   -input      <in_file>    file to process, has higher priority than hex\n"
		<< "   -hex-input  <in_file>    must be different from the output file name\n\n"
		<< "   -output     <out_file>   encoding|decoding file, has higher priority than hex\n"
		<< "   -hex-output <out_file>   must be different from the input file name\n\n"
		<< "Example:\n"
		<< "   " << appName << ' ' << testName << " -input file1 -hex-output file2\n"
		<< "   " << appName << ' ' << testName << " --decode -phrase -output file2 -input file3\n"
		<< "   " << appName << ' ' << testName << " --encode -file file1 -hex-input file2 -output file3\n"
		<< std::endl;
}
std::tuple<bool, Params, std::string> Params::parseCommandLine (add_cref<strings> cmdLineParams, ostream_ref log)
{
	const Option	optPhrase		{ "-phrase",     0 };
	const Option	optFile			{ "-file",       1 };
	const Option	optEncode		{ "--encode",    0 };
	const Option	optDecode		{ "--decode",    0 };
	const Option	optInput		{ "-input",      1 };
	const Option	optOutput		{ "-output",     1 };
	const Option	optHexInput		{ "-hex-input",  1 };
	const Option	optHexOutput	{ "-hex-output", 1 };
	const std::vector<Option> opts { optPhrase, optFile, optEncode, optDecode,
										optInput, optOutput, optHexInput, optHexOutput };
	Params	params;
	strings sink, args	(cmdLineParams);
	std::stringstream	errColl;

	auto makeResult = [&](bool result) -> std::tuple<bool, Params, std::string>
	{
		errColl.flush();
		return { result, params, errColl.str() };
	};

	// Encode <:> Decode
	{
		params.action = (consumeOptions(optEncode, opts, args, sink) > 0) ? Action::Encode : Action::None;
		if ((consumeOptions(optDecode, opts, args, sink) > 0) && (Action::None != params.action))
		{
			errColl << "There must be " << optEncode.name << " or " << optDecode.name << " specified once" << std::endl;
			return makeResult(false);
		}
		params.action = params.action == Action::None ? Action::Encode : params.action;
	}

	// Input
	{
		if (consumeOptions(optInput, opts, args, sink) > 0) params.input = sink.back();
		if (params.flags.hex_input = ((consumeOptions(optHexInput, opts, args, sink) > 0) && params.input.empty())  ? 1 : 0;
				0 != params.flags.hex_input) params.input = sink.back();

		const fs::path inputPath(params.input);
		if (!fs::is_regular_file(inputPath))
		{
			errColl << "Input file must exist" << std::endl;
			return makeResult(false);
		}
		const auto inputSize = fs::file_size(inputPath);
		if ((0 != params.flags.hex_input) && ((inputSize % 2u) != 0u))
		{
			errColl << "If -hex-input is specified then input file size must be a multiplication of two" << std::endl;
			return makeResult(false);
		}
	}

	// Output
	{
		if (consumeOptions(optOutput, opts, args, sink) > 0) params.output = sink.back();
		if (params.flags.hex_output = ((consumeOptions(optHexOutput, opts, args, sink) > 0) && params.output.empty())  ? 1 : 0;
				0 != params.flags.hex_output) params.output = sink.back();

		if (fs::path(params.output) == fs::path(params.input))
		{
			errColl << "Output file must not point to the input file" << std::endl;
			return makeResult(false);
		}
	}

	// Source of ciphering
	{
		params.source = (consumeOptions(optPhrase, opts, args, sink) > 0) ? Source::Phrase : Source::None;
		if (consumeOptions(optFile, opts, args, sink) > 0)
		{
			if (Source::Phrase == params.source)
			{
				errColl << "Phrase ciphering or file might be specified, not both" << std::endl;
				return makeResult(false);
			}
			params.source = Source::File;
			params.cipher = sink.back();
			const fs::path cipherPath(params.cipher);
			if (params.cipher.empty() || !fs::is_regular_file(cipherPath))
			{
				errColl << "Cipher file must exist" << std::endl;
				if (fs::path(params.output) == cipherPath)
					errColl << "Output file must not point to the cipher file" << std::endl;
				return makeResult(false);
			}
		}
	}

	if (!args.empty())
	{
		errColl << "Unrecognized one or more parameter(s): \"" << args.at(0) << "\"" << std::endl;
		return makeResult(false);
	}

	if (Source::File != params.source)
	{
		params.source = Source::Phrase;
		params.cipher = readPhrase(log, errColl);
		if (params.cipher.empty())
		{
			params.source = Source::None;
			return makeResult(false);
		}
	}

	return makeResult(true);
}
std::string Params::readPhrase (ostream_ref log, add_ref<std::stringstream> errColl)
{
	struct DisableEcho
	{
#if SYSTEM_OS_LINUX == 1
		termios before, after;
		DisableEcho ()
		{
			//tcgetattr(0, &before);
			//after = before;
			//after.c_lflag &= make_unsigned(~ICANON);
			//after.c_lflag &= make_unsigned(~ECHO);
			//tcsetattr(0, TCSANOW, &after);
		}
		~DisableEcho ()
		{
			//tcsetattr(0, TCSANOW, &before);
		}
#else
		DWORD before, after;
		DisableEcho ()
		{
			HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
			BOOL status = GetConsoleMode(hConsole, &before);
			UNREF(status);
			after = before;
			after &= ~ENABLE_ECHO_INPUT;
			after &= ~ENABLE_LINE_INPUT;
			SetConsoleMode(hConsole, after);
		}
		~DisableEcho()
		{
			HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
			SetConsoleMode(hConsole, before);
		}
#endif
	}			disableEcho;

	SIDE_EFFECT(disableEcho);
	std::string phrase1, phrase2;

	log << "Enter phrase (must not be empty, note that no echo)" << std::endl;
	log << "Phrase: ";
	std::cin >> phrase1;

	if (phrase1.empty())
	{
		errColl << "Phrase must not be empty" << std::endl;
		return phrase1;
	}

	log << std::endl << "Reenter phrase again (must match the previous one, no echo)" << std::endl;
	log << "Confirm phrase: ";
	std::cin >> phrase2;

	if (phrase1 != phrase2)
	{
		errColl << "Phrases must match" << std::endl;
		return std::string();
	}

	log << std::endl;
	return phrase2;
}

TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams);
TriLogicInt runIntComputeSingleThread (cstring_ref assets, add_cref<Params> params);
TriLogicInt prepareTests (add_cref<TestRecord> record, add_cref<strings> cmdLineParams)
{
	const auto [status, params, errorString] =
			Params::parseCommandLine(cmdLineParams, std::cout);
	TriLogicInt result(-1);
	if (status)
		result = runIntComputeSingleThread(record.assets, params);
	else
	{
		Params::usage(fs::path(getGlobalAppFlags().thisAppPath)
					  .filename().generic_u8string().c_str(), record.name, std::cout);
		std::cout << errorString << std::endl;
	}
	return result;
}
struct Reader
{
	add_ref<std::ifstream>	i;
	const bool				r;
	Reader (add_ref<std::ifstream>, bool rewindOnEof);
	void rewind ();
	static std::error_code isCharOrError (const int character);
	virtual int read ();
};
Reader::Reader (add_ref<std::ifstream> inFile, bool rewindOnEof)
	: i(inFile), r(rewindOnEof)
{
}
void Reader::rewind ()
{
	if (i.peek() == std::char_traits<char>::eof()) {
		if (r) i.seekg(0);
		if (r && i.peek() == std::char_traits<char>::eof())
			throw std::make_error_code(std::errc::io_error);
	}
}
std::error_code Reader::isCharOrError (const int character)
{
	return std::error_code((character >> __CHAR_BIT__), std::generic_category());
}
int Reader::read ()
{
	int x = 0;
	try
	{
		rewind();
		x = i.get();
		if (std::char_traits<char>::eof() == x)
		{
			throw std::make_error_code(std::errc::io_error);
		}
	}
	catch (std::error_code c)
	{
		x = c.value() << __CHAR_BIT__;
	}
	return x;
}
struct HexReader : Reader
{
	union {
		int32_t x;
		char s[sizeof(x)];
	} u;
	std::stringstream		ss;
	add_ref<int>			x;
	add_ref<char>			h;
	add_ref<char>			l;
	add_cptr<char>			s;
	HexReader (add_ref<std::ifstream>, bool rewindOnEof);
	virtual int read () override;
	void validateDigit (int);
};
HexReader::HexReader (add_ref<std::ifstream> inFile, bool rewindOnEof)
	: Reader (inFile, rewindOnEof)
	, u		{}
	, ss	("00 ")
	, x		(u.x)
	, h		(u.s[0])
	, l		(u.s[1])
	, s		(&u.s[0])
{
}
void HexReader::validateDigit (const int digit)
{
	if (digit == std::char_traits<char>::eof())
		throw std::make_error_code(std::errc::io_error);
	if (false == std::isxdigit(digit))
		throw std::make_error_code(std::errc::illegal_byte_sequence);
}
int HexReader::read ()
{
	int k = 0, L = k, H = k;
	try
	{
		rewind();
		H = i.get();	validateDigit(H);
		L = i.get();	validateDigit(L);

		x = 0;
		h = static_cast<char>(H);
		l = static_cast<char>(L);

		ss.seekp(0);
		ss << s;

		ss.seekg(0);
		ss >> std::hex >> k;

		x = k;
	}
	catch (std::error_code c)
	{
		x = c.value() << __CHAR_BIT__;
	}
	return x;
}
bool xorByPhrase (add_cref<Params> params, string_ref msg)
{
	bool result = false;
	std::ifstream input(params.input);
	std::ofstream output(params.output);
	auto makeMessage = [&](const auto&... x) -> void
	{
		std::ostringstream os;
		SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << x)...));
		os.flush();
		msg = os.str();
	};
	auto streamAppend = [&](auto& os, const auto&... x) -> auto&
	{
		SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << x)...));
		os.flush();
		return os;
	};
	if (input.is_open() && output.is_open())
	{
		result = true;
		const auto cipherLength = params.cipher.length();

		if (params.flags.hex_input)
		{
			HexReader rdr(input, false);

			for (uint32_t i = 0u; (input.peek() != std::char_traits<char>::eof()); ++i)
			{
				const int c = rdr.read();
				if (const std::error_code e = HexReader::isCharOrError(c); e.value() != 0)
				{
					makeMessage("Input file thrown:", e.message());
					result = false;
					break;
				}

				const int x = (c ^ params.cipher[i % cipherLength]);
				if (params.flags.hex_output == 0)
					output << static_cast<char>(x);
				else output << std::setw(2) << std::setfill('0') << std::hex << x;
			}
		}
		else
		{
			std::istreambuf_iterator<char> end, begin(input);
			for (auto i = begin; i != end; ++i)
			{
				const int x = (*i) ^ params.cipher[make_unsigned(std::distance(begin, i)) % cipherLength];
				if (params.flags.hex_output == 0)
					output << static_cast<char>(x);
				else output << std::setw(2) << std::setfill('0') << std::hex << x;
			}
		}
	}
	else
	{
		std::ostringstream ss;
		if (!input.is_open())	streamAppend(ss, "Unable to open", std::quoted(params.input), "for reading\n");
		if (!output.is_open())	streamAppend(ss, "Unable to open", std::quoted(params.output), "for writing\n");
		ss.flush();
		msg = ss.str();
	}
	return result;
}
bool xorByFile (add_cref<Params> params, string_ref msg)
{
	bool result = false;
	std::ifstream cipher(params.cipher);	const int cBit = 0;
	std::ifstream input(params.input);		const int iBit = 1;
	std::ofstream output(params.output);	const int oBit = 2;
	const uint32_t status = 0u
			| (cipher.is_open()	? (1u << cBit) : 0u)
			| (input.is_open()	? (1u << iBit) : 0u)
			| (output.is_open()	? (1u << oBit) : 0u);
	auto makeMessage = [&](const auto&... x) -> void
	{
		std::ostringstream os;
		SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << x)...));
		os.flush();
		msg = os.str();
	};
	auto streamAppend = [&](auto& os, const auto&... x) -> auto&
	{
		SIDE_EFFECT(std::forward_as_tuple((os << ((os.tellp() > 0) ? " " : "") << x)...));
		os.flush();
		return os;
	};
	if (status == ((1u << cBit) | (1u << iBit) | (1u << oBit)))
	{
		result = true;
		Reader			rdrCipher	(cipher, true);
		HexReader		hexInput	(input, false);
		Reader			charInput	(input, false);
		add_ref<Reader>	rdrInput	= params.flags.hex_input ? hexInput : charInput;

		while (input.peek() != std::char_traits<char>::eof())
		{
			const int c = rdrCipher.read();
			if (const std::error_code e = Reader::isCharOrError(c); e.value() != 0)
			{
				makeMessage("Cipher file thrown:", e.message());
				result = false;
				break;
			}
			const int i = rdrInput.read();
			if (const std::error_code e = Reader::isCharOrError(i); e.value() != 0)
			{
				makeMessage("Input file thrown:", e.message());
				result = false;
				break;
			}
			if (params.flags.hex_output == 0)
				output << static_cast<char>(c ^ i);
			else output << std::setw(2) << std::setfill('0') << std::hex << (c ^ i);
		}
	}
	else
	{
		std::ostringstream ss;
		if (!(status & (1u << cBit)))	streamAppend(ss, "Unable to open", std::quoted(params.cipher), "for reading\n");
		if (!(status & (1u << iBit)))	streamAppend(ss, "Unable to open", std::quoted(params.input), "for reading\n");
		if (!(status & (1u << oBit)))	streamAppend(ss, "Unable to open", std::quoted(params.output), "for writing\n");
		ss.flush();
		msg = ss.str();
	}
	return result;
}
bool assertParams (add_cref<Params>, string_ref)
{
	//
	return false;
}
auto assertParams (Params::Method) -> bool (&)(add_cref<Params>, string_ref) { return assertParams; }
auto decodeByPhrase (Params::Method method) -> bool (&)(add_cref<Params>, string_ref)
{
	ASSERTMSG(Params::Method::None != method, "Unknown method");
	return xorByPhrase;
}
auto decodeByFile (Params::Method method) -> bool (&)(add_cref<Params>, string_ref)
{
	ASSERTMSG(Params::Method::None != method, "Unknown method");
	return xorByFile;
}
auto encodeByPhrase (Params::Method method) -> bool (&)(add_cref<Params>, string_ref)
{
	ASSERTMSG(Params::Method::None != method, "Unknown method");
	return xorByPhrase;
}
auto encodeByFile (Params::Method method) -> bool (&)(add_cref<Params>, string_ref)
{
	ASSERTMSG(Params::Method::None != method, "Unknown method");
	return xorByFile;
}
TriLogicInt runIntComputeSingleThread (cstring_ref assets, add_cref<Params> params)
{
	UNREF(assets);
	params.print(std::cout);
	std::string msg;
	// Let's do stress the compiler a bit
	try
	{
		switch (params.action)
		{
		case Params::Action::Encode:
			ASSERTMSG((params.source == Params::Source::File
					   ? encodeByFile(params.method)
					   : params.source == Params::Source::Phrase
						 ? encodeByPhrase(params.method)
						 : assertParams(Params::Method::None))(params, msg), msg);
			break;
		case Params::Action::Decode:
			ASSERTMSG((params.source == Params::Source::File
					   ? decodeByFile(params.method)
					   : params.source == Params::Source::Phrase
						 ? decodeByPhrase(params.method)
						 : assertParams(Params::Method::None))(params, msg), msg);
			break;
		default:
			ASSERTMSG(assertParams(params, msg), msg);
			break;
		}
	}
	catch (add_cref<std::exception>& e)
	{
		std::cout << e.what() << std::endl;
		return (-1);
	}

	return 0;
}

} // unnamed namespace

template<> struct TestRecorder<INT_CIPHER>
{
	static bool record (TestRecord&);
};
bool TestRecorder<INT_CIPHER>::record (TestRecord& record)
{
	record.name = "int_cipher";
	record.call = &prepareTests;
	return true;
}
