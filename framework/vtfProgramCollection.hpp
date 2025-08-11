#ifndef __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__
#define __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__

#include <string>
#include <optional>

#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfCUtils.hpp"

namespace vtf
{

struct _GlSpvProgramCollection
{
	enum StageToCode
	{
		shaderCode = 0,
		header,
		entryName,
		fileName,
		includePaths
	};

	typedef std::pair<VkShaderStageFlagBits, uint32_t> StageAndIndex;

	class ShaderLink
	{
		friend struct ShaderObjectCollection;
		VkShaderStageFlagBits	stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		uint32_t				index = INVALID_UINT32;
		add_ptr<ShaderLink>		prev = nullptr;
		add_ptr<ShaderLink>		next = nullptr;
		StageAndIndex		key () const { return { stage, index }; }
		add_ptr<ShaderLink>	head() const;
		uint32_t			count() const;
	};

	virtual ~_GlSpvProgramCollection () = default;
	_GlSpvProgramCollection (ZDevice device, add_cref<std::string> basePath = std::string());

	void addFromText (VkShaderStageFlagBits type, add_cref<std::string> code,
					  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main");
	bool addFromFile (VkShaderStageFlagBits type,
					  add_cref<std::string> fileName, add_cref<strings> includePaths = {},
					  add_cref<std::string> entryName = "main", bool verbose = true);
	// availailable after build
    auto getShaderCode (VkShaderStageFlagBits stage, uint32_t index, bool binORasm = true) const -> add_cref<std::vector<char>>;
	auto getShaderFile (VkShaderStageFlagBits stage, uint32_t index, bool inOrout = false) const -> add_cref<std::string>;
	auto getShaderEntry (VkShaderStageFlagBits stage, uint32_t index) const -> add_cref<std::string>;

protected:
	ZDevice				m_device;
	const std::string	m_basePath;
	const std::string	m_tempDir;
	std::map<VkShaderStageFlagBits, uint32_t> m_stageToCount;
	// [0]: glsl code, [1]: entry name, [2] file name, [3...]: include path(s)
	std::map<StageAndIndex, strings> m_stageToCode;
	std::map<StageAndIndex, std::string> m_stageToFileName;
    std::map<StageAndIndex, std::vector<char>> m_stageToAssembly;
    std::map<StageAndIndex, std::vector<char>> m_stageToBinary;

private:
	virtual auto addFromText (VkShaderStageFlagBits, add_cref<std::string>, add_cref<ShaderLink>,
							  add_cref<strings>, add_cref<std::string>) -> ShaderLink { return {}; }
	virtual auto addFromFile (VkShaderStageFlagBits, add_cref<std::string>, add_cref<ShaderLink>,
							  add_cref<strings>, add_cref<std::string>, bool) -> ShaderLink { return {}; };
};

struct ProgramCollection : _GlSpvProgramCollection
{
	ProgramCollection (ZDevice device, add_cref<std::string> basePath = std::string());
	void buildAndVerify (add_cref<Version> vulkanVer = Version(1,0), add_cref<Version> spirvVer = Version(1,0),
						 bool enableValidation = false, bool genDisassembly = false, bool buildAlways = false);
	// Uses information from GlobalAppFlags
	void buildAndVerify (bool buildAlways);
	auto getShader (VkShaderStageFlagBits stage, uint32_t index = 0u, bool verbose = true) const -> ZShaderModule;
};

VkShaderStageFlagBits shaderGetStage (ZShaderModule module);

} // namespace vtf

#endif // __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__
