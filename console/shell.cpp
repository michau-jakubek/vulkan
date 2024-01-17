#include "shell.hpp"

#if !STANDALONE_PROJECT_CONSOLE
#include "vtfCUtils.hpp"
#endif

Shell::Shell (std::ostream& output, OnCommand onCommand,
			  const std::string& helpMessage, const std::string& historyFile)
	: m_output		(output)
	, m_historyFile	(historyFile)
	, m_helpMessage	(helpMessage)
	, m_console		(output, "> ")
	, m_onCommand	(onCommand)
	, shortgQuitCmd	("q")
	, longQuitCmd	("quit")
	, exitCmd		("exit")
	, helpCmd		("help")
#if STANDALONE_PROJECT_CONSOLE
	, shellEnabled	(false)
#else
	, shellEnabled	(true)
	, shellCmd		("shell")
#endif
{
}
void Shell::processCommand (bool& doContinue, const strings& chunks, const std::string& cmdRest)
{
	if (chunks.size() != 0)
	{
		if (chunks[0] == shortgQuitCmd || chunks[0] == longQuitCmd || chunks[0] == exitCmd)
			doContinue = false;
		else if (chunks[0] == helpCmd)
		{
			m_output << m_helpMessage << std::endl;
		}
#if !STANDALONE_PROJECT_CONSOLE
		else if (chunks[0] == shellCmd)
		{
			if (cmdRest.empty())
				std::cout << "shell params seem to be empty!" << std::endl;
			else
			{
				bool status = false;
				const std::string cmdRes = vtf::captureSystemCommandResult(cmdRest.c_str(), status, '\n');
				std::cout << cmdRes;
			}
		}
#endif
		else if (m_onCommand)
		{
			m_onCommand(std::ref(doContinue), std::cref(chunks), std::ref(m_output));
		}
		else
		{
			doContinue = false;
		}
	}
}
void Shell::run ()
{
	strings chunks;
	bool	doContinue(true);

	m_console.load(m_historyFile);
	while (doContinue)
	{
		m_console.read();
		m_console >> chunks;
#if !STANDALONE_PROJECT_CONSOLE
		m_console.clear();
		m_console.seekg(0);
		std::pair<std::string, std::string> cmdAndRest;
		m_console >> cmdAndRest;
#endif
		processCommand(doContinue, chunks, cmdAndRest.second);
	}
	m_console.save(m_historyFile);
}
