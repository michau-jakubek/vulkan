#ifndef __RT_SHADEERS_COLLECTION_HPP_INCLUDED__
#define __RT_SHADEERS_COLLECTION_HPP_INCLUDED__

#include "vtfProgramCollection.hpp"
#include <map>

namespace vtf
{

struct RTShaderCollection : public _GlSpvProgramCollection
{
	using _GlSpvProgramCollection::_GlSpvProgramCollection;

	struct SBTShaderGroup : _GlSpvProgramCollection::RTShaderGroup
	{
		SBTShaderGroup (uint32_t groupIndex = 0u);
		SBTShaderGroup (add_cref<SBTShaderGroup> other);
		struct Less { bool operator () (add_cref<SBTShaderGroup>, add_cref<SBTShaderGroup>) const; };
		bool operator== (add_cref<SBTShaderGroup> other) const;
		bool operator!= (add_cref<SBTShaderGroup> other) const;
		SBTShaderGroup next () const;
	};

	void addFromText (add_cref<SBTShaderGroup> logicalGroup, VkShaderStageFlagBits stage, add_cref<std::string> code,
					  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main");
	bool addFromFile (add_cref<SBTShaderGroup> logicalGroup, VkShaderStageFlagBits stage,
					  add_cref<std::string> fileName, add_cref<strings> includePaths = {},
					  add_cref<std::string> entryName = "main", bool verbose = true);
	void addFromText (add_cref<SBTShaderGroup> logicalGroup, add_cref<SBTShaderGroup> hitGroup,
					  VkShaderStageFlagBits stage, add_cref<std::string> code,
					  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main");
	bool addFromFile (add_cref<SBTShaderGroup> logicalGroup, add_cref<SBTShaderGroup> hitGroup,
					  VkShaderStageFlagBits stage, add_cref<std::string> fileName, add_cref<strings> includePaths = {},
					  add_cref<std::string> entryName = "main", bool verbose = true);
	void buildAndVerify (add_cref<Version> vulkanVer = Version(1,0), add_cref<Version> spirvVer = Version(1,0),
						 bool enableValidation = false, bool genDisassembly = false, bool buildAlways = false,
						 add_cref<std::string> spirvValArgs = {}, uint32_t threads = 1u);
	// Uses information from GlobalAppFlags
	void buildAndVerify (bool buildAlways, uint32_t threads = 1);
	auto getAllShaders (std::initializer_list<SBTShaderGroup> = {/*empty means all*/}) const -> std::vector<ZShaderModule>;

protected:
	bool findShaderKey (add_cref<SBTShaderGroup>, add_cref<SBTShaderGroup>,
						VkShaderStageFlagBits, add_ref<StageAndIndex> key) const;
	ZShaderModule	makeShader (add_cref<StageAndIndex> key) const;
	ShaderLink		makeLink (add_cref<StageAndIndex> key) const;
	std::multimap<SBTShaderGroup, ShaderLink, SBTShaderGroup::Less> m_shaders;
};

} // namespace vtf

#endif // __RT_SHADEERS_COLLECTION_HPP_INCLUDED__
