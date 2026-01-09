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
		shaderOriginalCode = 0,
		header,
		entryName,
		fileName,
		includePaths
	};

	typedef std::pair<VkShaderStageFlagBits, uint32_t> StageAndIndex;

	class ShaderLink
	{
		friend struct RTShaderCollection;
		friend struct ShaderObjectCollection;
		VkShaderStageFlagBits	stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		uint32_t				index = INVALID_UINT32;
		add_ptr<ShaderLink>		prev = nullptr;
		add_ptr<ShaderLink>		next = nullptr;
		StageAndIndex		key () const { return { stage, index }; }
		uint32_t			count() const;
		add_ptr<ShaderLink>	head ();
	};

	virtual ~_GlSpvProgramCollection () = default;
	_GlSpvProgramCollection (ZDevice device, add_cref<std::string> basePath = std::string());
	uint32_t collectionID () const;

	// availailable after build
    auto getShaderCode (VkShaderStageFlagBits stage, uint32_t index, bool binORasm = true) const -> add_cref<std::vector<char>>;
	auto getShaderFile (VkShaderStageFlagBits stage, uint32_t index, bool inOrout = false) const -> add_cref<std::string>;
	auto getShaderEntry (VkShaderStageFlagBits stage, uint32_t index) const -> add_cref<std::string>;

protected:
	class RTShaderGroup
	{
		friend struct RTShaderCollection;
		const uint32_t m_groupIndex;
	public:
		RTShaderGroup(uint32_t groupIndex = 0u);
		uint32_t groupIndex() const;
	};

	auto _addFromText (VkShaderStageFlagBits type, add_cref<std::string> code,
					  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main",
					  add_cptr<StageAndIndex> hintKey = nullptr) -> StageAndIndex;
	auto _addFromFile (VkShaderStageFlagBits type,
					  add_cref<std::string> fileName, add_cref<strings> includePaths = {},
					  add_cref<std::string> entryName = "main", bool verbose = true,
					  add_cptr<StageAndIndex> hintKey = nullptr) -> StageAndIndex;
	void _buildAndVerify (add_cref<Version> vulkanVer = Version(1,0), add_cref<Version> spirvVer = Version(1,0),
						 bool enableValidation = false, bool genDisassembly = false, bool buildAlways = false,
						 add_cref<std::string> spirvValArgs = {}, uint32_t threads = 1u);

	ZDevice				m_device;
	const std::string	m_basePath;
	const std::string	m_tempDir;
	std::map<VkShaderStageFlagBits, uint32_t> m_stageToCount;
	// [0]: glsl code, [1]: entry name, [2] file name, [3...]: include path(s)
	std::map<StageAndIndex, strings> m_stageToCode;
	std::map<StageAndIndex, std::string> m_stageToFileName;
    std::map<StageAndIndex, std::vector<char>> m_stageToAssembly;
    std::map<StageAndIndex, std::vector<char>> m_stageToBinary;
	std::map<StageAndIndex, std::vector<char>> m_stageToDisassembly;

private:
	virtual auto addFromText (VkShaderStageFlagBits, add_cref<std::string>, VkShaderStageFlagBits,
							  add_cref<strings>, add_cref<std::string>) -> ShaderLink { return {}; }
	virtual auto addFromText (VkShaderStageFlagBits, add_cref<std::string>, add_cref<ShaderLink>,
							  add_cref<strings>, add_cref<std::string>) -> ShaderLink { return {}; }
	virtual auto addFromFile (VkShaderStageFlagBits, add_cref<std::string>, VkShaderStageFlagBits,
							  add_cref<strings>, add_cref<std::string>, bool) -> ShaderLink { return {}; };
	virtual auto addFromFile (VkShaderStageFlagBits, add_cref<std::string>, add_cref<ShaderLink>,
							  add_cref<strings>, add_cref<std::string>, bool) -> ShaderLink { return {}; };
	static uint32_t m_collectionID;
};

struct ProgramCollection : _GlSpvProgramCollection
{
	ProgramCollection (ZDevice device, add_cref<std::string> basePath = std::string());
	void addFromText (VkShaderStageFlagBits type, add_cref<std::string> code,
					  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main");
	bool addFromFile (VkShaderStageFlagBits type,
					  add_cref<std::string> fileName, add_cref<strings> includePaths = {},
					  add_cref<std::string> entryName = "main", bool verbose = true);
	void buildAndVerify (add_cref<Version> vulkanVer = Version(1,0), add_cref<Version> spirvVer = Version(1,0),
						 bool enableValidation = false, bool genDisassembly = false, bool buildAlways = false,
						 add_cref<std::string> spirvValArgs = {}, uint32_t threads = 1u);
	// Uses information from GlobalAppFlags
	void buildAndVerify (bool buildAlways, uint32_t threads = 1u);
	auto getShader (VkShaderStageFlagBits stage, uint32_t index = 0, bool verbose = true) const -> ZShaderModule;

	static std::vector<std::pair<std::string, std::string>> getAvailableCompilerList (bool glslangValidator);
};

VkShaderStageFlagBits shaderGetStage (ZShaderModule module);
std::string shaderGetStageString (ZShaderModule module);
std::string shaderStageToString (VkShaderStageFlagBits stage);

} // namespace vtf

#endif // __VTF_PROGRAM_COLLECTION_HPP_INCLUDED__
