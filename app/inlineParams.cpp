#include "vtfVkUtils.hpp"
#include "vtfZUtils.hpp"
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