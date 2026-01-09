#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfProgramCollection.hpp"
#include <iostream>

using namespace vtf;

static void printLayerExtensions (add_ref<std::ostream> str, add_cref<std::string> layerName, const int il)
{
	const strings e(enumerateInstanceExtensions(layerName.empty() ? nullptr : layerName.c_str()));

	if (layerName.empty())
		str << "Layer NULL, extensions: " << e.size() << std::endl;
	else
		str << il << ": " << layerName << " extensions " << e.size() << std::endl;
	if (e.size())
	{
		int ie = 0;
		for (add_cref<std::string> name : e)
			str << "   " << ie++ << ": " << name << std::endl;
	}
}

void printLayersAndExtensions (add_ref<std::ostream> str)
{
	printLayerExtensions(str, {}, 0);

	const strings layers(enumerateInstanceLayers());
	if (layers.empty())
	{
		str << "No other layers to present" << std::endl;
	}
	else
	{
		int i = 0;
		for (add_cref<std::string> name : layers)
			printLayerExtensions(str, name, i++);
	}

}

void printCompilerList (add_ref<std::ostream> str)
{
	std::string versionLine;
	auto compilers = ProgramCollection::getAvailableCompilerList(true);
    str << "Available compiler list consists of " << data_count(compilers) << " compiler(s):\n"
           "---------------------------------------------------\n";
	for (uint32_t cv = 0u; cv < data_count(compilers); ++cv)
	{
		add_ref<std::pair<std::string, std::string>> item = compilers.at(cv);
		str << "  " << cv << ": " << std::quoted(item.first) << '\n';
		str << "  Version:\n";
		std::istringstream version(std::move(item.second));
		while (std::getline(version, versionLine))
		{
			str << "    " << versionLine << '\n';
		}
	}
}
