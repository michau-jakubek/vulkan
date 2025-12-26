#ifndef __RT_SHADEERS_COLLECTION_HPP_INCLUDED__
#define __RT_SHADEERS_COLLECTION_HPP_INCLUDED__

#include "vtfProgramCollection.hpp"
#include <map>

namespace vtf
{

struct RTShaderCollection : public _GlSpvProgramCollection
{
	enum class HitGroupsOrder : uint16_t
	{
		InsertOrder = 1,
		NumberOrder = 2
	};
	using SBTShaderGroup = _GlSpvProgramCollection::RTShaderGroup;

	RTShaderCollection(ZDevice device, HitGroupsOrder = HitGroupsOrder::InsertOrder,
						add_cref<std::string> basePath = std::string());

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
						 add_cref<std::string> spirvValArgs = {});
	// Uses information from GlobalAppFlags
	void buildAndVerify (bool buildAlways);
	auto getAllShaders (add_cref<SBTShaderGroup>) const ->std::vector<ZShaderModule>;
	auto getAllShaders () const -> std::vector<ZShaderModule>;

protected:
	const HitGroupsOrder m_hitGroupsOrder;
	bool findShaderKey (add_cref<SBTShaderGroup>, add_cref<SBTShaderGroup>,
						VkShaderStageFlagBits, add_ref<StageAndIndex> key) const;
	ZShaderModule	makeShader (add_cref<StageAndIndex> key) const;
	ShaderLink		makeLink (add_cref<StageAndIndex> key) const;
	std::multimap<SBTShaderGroup, ShaderLink, SBTShaderGroup::Less> m_shaders;
};

} // namespace vtf

#endif // __RT_SHADEERS_COLLECTION_HPP_INCLUDED__
