#include <fstream>
#include <map>
#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfObjectLoader.hpp"

namespace vtf
{

struct FaceInfo
{
	std::vector<uint32_t>	vertices;
	std::vector<uint32_t>	coords;
	std::vector<uint32_t>	normals;
};

bool parseVertices (std::ifstream& str, ObjectFileContent& content, FaceInfo&, std::string& sinkLine)
{
	float x = 0.0f;	str >> x;
	float y = 0.0f;	str >> y;
	float z = 0.0f;	str >> z;
	content.vertices.emplace_back(x,y,z);
	std::getline(str, sinkLine);
	return true;
}

bool parseCoords (std::ifstream& str, ObjectFileContent& content, FaceInfo&, std::string& sinkLine)
{
	float u = 0.0f;	str >> u;
	float v = 0.0f; str >> v;
	content.coords.emplace_back(u,v);
	std::getline(str, sinkLine);
	return true;
}

bool parseNormals (std::ifstream& str, ObjectFileContent& content, FaceInfo&, std::string& sinkLine)
{
	float x = 0.0f;	str >> x;
	float y = 0.0f;	str >> y;
	float z = 0.0f;	str >> z;
	content.normals.emplace_back(x,y,z);
	std::getline(str, sinkLine);
	return true;
}

bool parseFace (std::ifstream& str, ObjectFileContent&, FaceInfo& fi, std::string& sinkLine)
{
	char sink = ' ';
	uint32_t v1 = 0; uint32_t v2 = 0; uint32_t v3 = 0;
	uint32_t c1 = 0; uint32_t c2 = 0; uint32_t c3 = 0;
	uint32_t n1 = 0; uint32_t n2 = 0; uint32_t n3 = 0;

	str >> v1; str >> sink; str >> c1; str >> sink; str >> n1;
	str >> v2; str >> sink; str >> c2; str >> sink; str >> n2;
	str >> v3; str >> sink; str >> c3; str >> sink; str >> n3;

	std::getline(str, sinkLine);

	fi.vertices.push_back(v1);
	fi.vertices.push_back(v2);
	fi.vertices.push_back(v3);

	fi.coords.push_back(c1);
	fi.coords.push_back(c2);
	fi.coords.push_back(c3);

	fi.normals.push_back(n1);
	fi.normals.push_back(n2);
	fi.normals.push_back(n3);

	return true;
}

std::map<char, bool (*)(std::ifstream&, ObjectFileContent&, FaceInfo&, std::string&)>
typeSwitch
{
	{ 'v', &parseVertices	},
	{ 't', &parseCoords		},
	{ 'n', &parseNormals	},
	{ 'f', &parseFace		}
};

bool parseObjectFile (const std::string& objectFile, ObjectFileContent& content)
{
	std::ifstream	str(objectFile);
	if (!str.is_open())
	{
		content.error = "Unable to open " + objectFile;
		return false;
	}

	auto readType = [&]()
	{
		str >> std::noskipws;
		char type = ' ';
		char typeNext = ' ';
		str >> type;
		if ('v' == type)
		{
			str >> typeNext;
			if (' ' != typeNext)
				type = typeNext;
		}
		str >> std::skipws;
		return type;
	};

	bool				ok = true;
	std::string			sinkLine;
	ObjectFileContent	tmp;
	FaceInfo			face;

	while (!str.bad() && !str.eof())
	{
		char type = readType();
		if (mapHasKey(typeSwitch, type))
			ok = (*typeSwitch.at(type))(str, tmp, face, sinkLine);
		else
		{
			std::getline(str, sinkLine);
		}
	}

	ok = !str.bad();
	str.close();

	if (!ok)
	{
		content.error = "Invalid operation";
		return false;
	}

	{
		const size_t vertexIndicesSize = face.vertices.size();
		const size_t verticesSize = tmp.vertices.size();
		for (size_t i = 0; i < vertexIndicesSize; ++i)
		{
			const uint32_t index = face.vertices[i];
			if (index == 0 || index > verticesSize)
			{
				content.error = "Bad vertex index";
				return false;
			}
			content.vertices.push_back(tmp.vertices.at(index - 1));
		}
	}

	{
		const size_t coordIndicesSize = face.coords.size();
		const size_t coordsSize = tmp.coords.size();
		for (size_t i = 0; i < coordIndicesSize; ++i)
		{
			const uint32_t index = face.coords[i];
			if (index == 0 || index > coordsSize)
			{
				content.error = "Bad coord index";
				return false;
			}
			content.coords.push_back(tmp.coords.at(index - 1));
		}
	}

	{
		const size_t normalIndicesSize = face.normals.size();
		const size_t normalsSize = tmp.normals.size();
		for (size_t i = 0; i < normalIndicesSize; ++i)
		{
			const uint32_t index = face.normals[i];
			if (index == 0 || index > normalsSize)
			{
				content.error = "Bad normal index";
				return false;
			}
			content.normals.push_back(tmp.normals.at(index - 1));
		}
	}

	return true;
}

} // namespace vtf
