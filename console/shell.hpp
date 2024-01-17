#ifndef __INTERACTIVE_CONSOLE_SHELL_HPP__
#define __INTERACTIVE_CONSOLE_SHELL_HPP__

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "console.hpp"

class Shell
{
	typedef std::vector<std::string> Strings;
	typedef std::function<void(bool& doContinue, const Strings& chunks, std::ostream& output)> OnCommandCb;
	std::ostream&		m_output;
	const std::string	m_historyFile;
	std::string			m_helpMessage;
	icon::iconstream	m_console;
	OnCommandCb			m_onCommand;
public:
	typedef Strings strings;
	typedef OnCommandCb OnCommand;
	const std::string	shortgQuitCmd;
	const std::string	longQuitCmd;
	const std::string	exitCmd;
	const std::string	helpCmd;
	const bool			shellEnabled;
#if !STANDALONE_PROJECT_CONSOLE
	const std::string	shellCmd;
#endif
	Shell (std::ostream& output, OnCommand onCommand,
		   const std::string& helpMessage = "This is help message", const std::string& historyFile = {});
	virtual ~Shell () = default;

	const std::string&	helpMessage () const					{ return m_helpMessage; }
	void				helpMessage (const std::string& msg)	{ m_helpMessage = msg; }

	void run ();
private:
	void processCommand (bool& doContinue, const strings& chunks, const std::string& cmdRest);
};

#endif // #ifndef __INTERACTIVE_CONSOLE_SHELL_HPP__
