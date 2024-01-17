#include <iomanip>
#include <iostream>
#include "shell.hpp"

int main(int argc, char** argv)
{
	auto onCommand = [](bool& doContinue, const Shell::strings& chunks, std::ostream& output) -> void
	{
		(void)(doContinue);
		for (uint32_t i = 0; i < chunks.size(); ++i)
		{
			if (i > 0) output << ' ';
			output << std::quoted(chunks.at(i));
		}
		output << std::endl;
	};

	const std::string history(argc > 1 ? argv[1] : "");
	Shell shell(std::cout, onCommand, "This is help message", history);
	shell.run();

	std::cout << "Done..." << std::endl;

    return 0;
}

