#ifndef __ZSHADER_OBJECT_HPP_INCLUDED__
#define __ZSHADER_OBJECT_HPP_INCLUDED__

#include "vtfProgramCollection.hpp"
#include "vtfZShaderObject.hpp"

namespace vtf
{

struct ShaderObjectCollection : _GlSpvProgramCollection
{
	using _GlSpvProgramCollection::ShaderLink;

	ShaderObjectCollection (ZDevice device, add_cref<std::string> basePath = std::string(), uint32_t maxShaders = 64u);

	virtual auto addFromText (VkShaderStageFlagBits type, add_cref<std::string> code, add_cref<ShaderLink> parent = {},
							  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main") -> ShaderLink override;
	virtual auto addFromFile (VkShaderStageFlagBits type, add_cref<std::string> fileName, add_cref<ShaderLink> parent = {},
							  add_cref<strings> includePaths = {}, add_cref<std::string> entryName = "main",
							  bool verbose = true) -> ShaderLink override;

	// If version(s) is valid then it will be applied
	// otherwise they will be taken from buildAndVerify() method parameters.
	void updateShader (add_cref<ShaderLink> link,
					   bool buildAlways = false, bool enableValidation = false, bool genAssembly = false,
					   add_cref<Version> vulkanVer = Version(0u, 0u),
					   add_cref<Version> spirvVer = Version(0u, 0u));
	void updateShaders(std::initializer_list<ShaderLink> links,
					   bool buildAlways = false, bool enableValidation = false, bool genAssembly = false,
					   add_cref<Version> vulkanVer = Version(0u, 0u),
					   add_cref<Version> spirvVer = Version(0u, 0u));
	void updateShader (add_cref<ShaderLink> link, std::initializer_list<ZDescriptorSetLayout> layouts);
	void updateShaders(std::initializer_list<ShaderLink> links,
					   std::initializer_list<ZDescriptorSetLayout> layouts);
	template<class X, class... Y>
		void updateShader (add_cref<ShaderLink> link, const ZSpecEntry<X>& entry, const ZSpecEntry<Y>&... entries);

	void updateShaders (std::initializer_list<ShaderLink> links, add_cref<ZPushConstants> pushConstants);
	template<class X, class... Y>
		void updateShaders (std::initializer_list<ShaderLink> links, const ZPushRange<X>& range, const ZPushRange<Y>&... ranges);
	template<class... Y>
		void updateShaders (std::initializer_list<ShaderLink> links, const std::tuple<Y...>& types,
							VkShaderStageFlags flagsForAllRanges = VK_SHADER_STAGE_ALL);
	void buildAndVerify (add_cref<Version> vulkanVer = Version(1, 3), add_cref<Version> spirvVer = Version(1, 3));

	ZShaderObject getShader (VkShaderStageFlagBits stage, uint32_t index = 0);

protected:
	struct ShaderData;
	struct ZShaderCreateInfoEXT;
	struct IntShaderLink : ShaderLink
	{
		add_ref<ShaderObjectCollection> collection;
		add_ptr<ShaderData> data;
		const bool autoDelete;

		~IntShaderLink ();
		IntShaderLink (IntShaderLink&& other) noexcept;
		IntShaderLink (std::nullptr_t, add_cref<IntShaderLink> other) noexcept;
		IntShaderLink (add_ref<ShaderObjectCollection>, VkShaderStageFlagBits, uint32_t);
	};

	auto find	(VkShaderStageFlagBits type, uint32_t shaderIndex, add_ref<std::vector<IntShaderLink>>) -> add_ptr<ShaderLink>;
	auto add	(VkShaderStageFlagBits type, uint32_t shaderIndex, add_cref<ShaderLink> parent) -> ShaderLink;
	auto spec	(add_cref<ShaderLink> link) -> add_ref<ZSpecializationInfo>;
	auto pushc	(add_cref<ShaderLink> link) -> add_ref<ZPushConstants>;
	bool create	(add_cref<Version> vulkanVer, add_cref<Version> spirvVer, add_ref<IntShaderLink> link);
	int  create (add_cref<Version> vulkanVer, add_cref<Version> spirvVer, add_ref<std::vector<IntShaderLink>> links);

	const uint32_t				m_maxShaders;
	uint32_t					m_linkCount;
	std::vector<IntShaderLink>	m_links;
};


template<class X, class... Y> inline void
ShaderObjectCollection::updateShaders (
	std::initializer_list<ShaderLink> links,
	const ZPushRange<X>&	range,
	const ZPushRange<Y>&... ranges)
{
	updateShaders(links, ZPushConstants(range, ranges...));
}

template<class... Y> void
ShaderObjectCollection::updateShaders (
	std::initializer_list<ShaderLink> links,
	const std::tuple<Y...>&	types,
	VkShaderStageFlags		flagsForAllRanges)
{
	UNREF(types);
	updateShaders(links, ZPushConstants(ZPushRange<Y>(flagsForAllRanges)...));
}

template<class X, class... Y> inline void
ShaderObjectCollection::updateShader (add_cref<ShaderLink> link, const ZSpecEntry<X>& entry, const ZSpecEntry<Y>&... entries)
{
	add_ref<ZSpecializationInfo> info = spec(link);
	info.clear();
	info.addEntries(entry, entries...);
}

namespace so
{
} // namespace so

} // namespace vtf

#endif // __ZSHADER_OBJECT_HPP_INCLUDED__
