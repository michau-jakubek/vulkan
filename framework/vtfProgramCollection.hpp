#ifndef __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__
#define __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__

#include <string>
#include <optional>

#include "vulkan/vulkan.h"
#include "vtfZDeletable.hpp"
#include "vtfContext.hpp"

namespace vtf
{

struct VerifyShaderCodeInfo
{
	const std::string			code_;
	const VkShaderStageFlagBits	stage_;
	VerifyShaderCodeInfo (const std::string& code, VkShaderStageFlagBits stage)
		: code_(code), stage_(stage) {}
};
bool verifyShaderCodeGL (const VerifyShaderCodeInfo* infos, uint32_t infoCount, std::string& error, int majorVersion=4, int minorVersion=5, void* glWindow = nullptr);

class ProgramCollection
{
	VulkanContext&		m_context;
	const std::string	m_basePath;
	const std::string	m_tempDir;
	std::map<VkShaderStageFlagBits, strings> m_stageToCode; // [0]: glsl code, [1]: entry name, [2...]: include path(s)
	std::map<VkShaderStageFlagBits, std::vector<unsigned char>> m_stageToBinary;
public:
	ProgramCollection (VulkanContext& vc, const std::string& basePath = std::string());
	void add (VkShaderStageFlagBits type, const std::string& code, const strings& includePaths = {}, const std::string& entryName = "main");
	bool addFromFile(VkShaderStageFlagBits type,
					 const std::string& fileName, const strings& includePaths = {},
					 const std::string& entryName = "main", bool verbose = true);
	void buildAndVerify (const Version& vulkanVer = Version(1,0), const Version& spirvVer = Version(1,0), bool enableValidation = false, bool buildAlways = false);
	std::optional<ZShaderModule>	getShader (VkShaderStageFlagBits stage, bool verbose = false) const;
};

void addProgram (ProgramCollection& programCollection,	const std::string& programName,		VkShaderStageFlagBits programType,
				 const std::string& glslFileName,		const std::string& spirvFileName,
				 const std::string& glslSource,			const std::string& spirvSource,
				 bool				verbose = true);

} // namespace vtf

#endif // __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__
