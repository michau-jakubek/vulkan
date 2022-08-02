
#ifndef __VTF_OBJECT_LOADER_HPP_INCLUDED__
#define __VTF_OBJECT_LOADER_HPP_INCLUDED__

#include "vtfVector.hpp"

namespace vtf
{
struct ObjectFileContent
{
	std::vector<Vec3>	vertices;
	std::vector<Vec2>	coords;
	std::vector<Vec3>	normals;
	std::string			error;
};
bool parseObjectFile (const std::string& objectFile, ObjectFileContent& content);
} // vtf

#endif // __VTF_OBJECT_LOADER_HPP_INCLUDED__
