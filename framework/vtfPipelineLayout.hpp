#ifndef __VTF_PIPELINE_LAYOUT_HPP_INCLUDED__
#define __VTF_PIPELINE_LAYOUT_HPP_INCLUDED__

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <typeindex>
#include <type_traits>
#include <variant>
#include "vtfZDeletable.hpp"
#include "vtfContext.hpp"

/*
 * +===========================================+=====================+==============================+
 * | Descriptor Type                           | shader access       | GLSL functions example       |
 * +===========================================+=====================+==============================+
 * | VK_DESCRIPTOR_TYPE_STORAGE_IMAGE          | uniform image2D u   | imageLoad|Store(u, ...)      |
 * +-------------------------------------------+---------------------+------------------------------+
 * | VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE          | uniform texture2D t |                              |
 * |                                           | uniform sampler   s | texture(sampler2D(t,s), ...) |
 * +-------------------------------------------+---------------------+------------------------------+
 * | VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER | uniform sampler2D s | texture(s, ...)              |
 * +-------------------------------------------+---------------------+------------------------------+
 */
namespace vtf
{

struct DescriptorImageInfo
{
	std::optional<ZSampler>	sampler;
	ZImageView				view;
	ZImage					image;
	VkImageLayout			layout;
	DescriptorImageInfo (std::optional<ZSampler>	sampler_,
						 ZImageView					view_,
						 ZImage						image_,
						 VkImageLayout				layout_)
		: sampler	(sampler_)
		, view		(view_)
		, image		(image_)
		, layout	(layout_) {}
};
struct DescriptorBufferInfo
{
	ZBuffer			buffer;
	VkDeviceSize    offset;
	VkDeviceSize    range;
	DescriptorBufferInfo (ZBuffer		buffer_,
						  VkDeviceSize	offset_,
						  VkDeviceSize	range_)
		: buffer(buffer_)
		, offset(offset_)
		, range	(range_) { }
};
typedef std::variant<std::monostate, DescriptorBufferInfo, DescriptorImageInfo>	VarDescriptorInfo;

class PipelineLayout
{
public:
											PipelineLayout				(VulkanContext& context);
	VulkanContext&							context						() const { return m_context; }
	/**
	 * Creates single buffer and concatenates all chunks into it
	 * that have the same descriptor type and buffer usage.
	 */
	template<class X> uint32_t				joinBindings				(VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
																		 VkShaderStageFlags stages = VK_SHADER_STAGE_ALL, uint32_t count = 1);
	/**
	 * Creates a separate binding, usefull for vectors SSBO.
	 * In the case where X is a std::vector<Y> an elementCount is the element count of that vector.
	 * In the case that X is standalone type (e.q. user structure) the elementCount param is ignored.
	 */
	template<class VecOrElemT> uint32_t		addBinding					(uint32_t elementCount = 1, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	/**
	 * Basically does the same things like addBinding().
	 * The one difference is that it automatically treat Y as std::vector<Y>
	 */
	template<class ElemType> uint32_t		addBindingAsVector			(uint32_t elementCount = 1, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	template<class ElemType> uint32_t		addBindingAsVector			(const std::vector<ElemType>& vector, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	uint32_t								addBinding					(std::optional<ZImageView> view, std::optional<ZSampler> sampler = {},
																		 bool forceStorageImageIfStorageUsageIsSet = false,
																		 VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	template<class X> std::optional<X>		getBinding					(uint32_t binding) const;
	template<class Data__> void				writeBinding				(uint32_t binding, const Data__& data);
	void									zeroBinding					(uint32_t binding);
	template<class Data__> void				readBinding					(uint32_t binding, Data__& data) const;
	VarDescriptorInfo						getDescriptorInfo			(uint32_t binding);
	ZDescriptorSetLayout					createDescriptorSetLayout	();
	ZDescriptorSet							getDescriptorSet			(ZDescriptorSetLayout dsLayout);
	ZPipelineLayout							createPipelineLayout		();
	ZPipelineLayout							createPipelineLayout		(ZDescriptorSetLayout dsLayout);
	template<class PC__> ZPipelineLayout	createPipelineLayout		(VkShaderStageFlags pcStageFlags = VK_SHADER_STAGE_ALL);
	template<class PC__> ZPipelineLayout	createPipelineLayout		(ZDescriptorSetLayout dsLayout, VkShaderStageFlags pcStageFlags = VK_SHADER_STAGE_ALL);

private:	
	typedef struct VkDescriptorSetLayoutBindingAndType : VkDescriptorSetLayoutBinding
	{
		VkDescriptorSetLayoutBindingAndType()
			: index			(typeid(void))
			, size			(0)
			, offset		(0)
			, elementCount	(1)
			, isVector		(false)
			, shared		(false) {}
		std::type_index index;
		VkDeviceSize	size;
		VkDeviceSize	offset;
		uint32_t		elementCount;
		bool			isVector;
		bool			shared;
	} ExtBinding;
	typedef std::vector<ExtBinding>	ExtBindings;

	uint32_t								joinBindings_				(std::type_index index, VkDeviceSize size, bool isVector,
																		 VkDescriptorType type, VkShaderStageFlags stages, uint32_t count);
	uint32_t								addBinding_					(std::type_index index, VkDeviceSize size, bool isVector,
																		 VkDescriptorType type, VkShaderStageFlags stages, uint32_t elementCount);
	const ExtBinding&						verifyGetExtBinding			(uint32_t binding) const;
	const ExtBinding&						verifyGetExtBinding			(std::type_index index, uint32_t binding) const;
	void									writeBinding_				(std::type_index index, uint32_t binding, const uint8_t* data, VkDeviceSize bytes);
	void									readBinding_				(std::type_index index, uint32_t binding, uint8_t* data, VkDeviceSize bytes) const;
	void									getBinding_					(uint32_t binding, std::optional<ZImageView>&) const;
	void									getBinding_					(uint32_t binding, std::optional<ZSampler>&) const;
	void									getBinding_					(uint32_t binding, std::optional<std::pair<ZImageView,ZSampler>>&) const;
	uint32_t								getDescriptorAlignment		(const VkDescriptorType type) const;
	void									updateBuffersOffsets		();
	void									recreateUpdateBuffers		(std::map<std::pair<VkDescriptorType, int>, ZBuffer>&	buffers);
	void									updateDescriptorSet			(ZDescriptorSet	descriptorSet,
																		 std::map<std::pair<VkDescriptorType, int>, ZBuffer>&	buffers) const;
	ZPipelineLayout							createPipelineLayout_		(std::initializer_list<ZDescriptorSetLayout> descriptorSetLayouts,
																		 const VkPushConstantRange* pPushConstantRanges, std::type_index typeOfPushConstant);

private:
	VulkanContext&												m_context;
	ZDevice														m_device;
	ExtBindings													m_extbindings;
	std::vector<ZImageView>										m_views;
	std::vector<ZSampler>										m_samplers;
	std::vector<std::pair<ZImageView,ZSampler>>					m_viewsAndSamplers;
	//std::vector<ZBuffer>										m_texelBuffers;
	std::map<std::pair<VkDescriptorType, int>, ZBuffer>			m_buffers;
	std::unique_ptr<VkPhysicalDeviceProperties>					m_properties;
	const uint32_t												m_minUniformBufferOffsetAlignment;
	const uint32_t												m_minStorageBufferOffsetAlignment;
	const uint32_t												m_maxUniformBufferRange;
	const uint32_t												m_maxStorageBufferRange;
};
namespace hidden
{
template<class X> struct BoundType
{
	static constexpr bool isVector () { return false; }
	static constexpr VkDeviceSize size () { return sizeof(X); }
	static VkDeviceSize length (const X&) { return sizeof(X); }
	static uint8_t* data (X& x) { return reinterpret_cast<uint8_t*>(&x); }
	static const uint8_t* data (const X& x) { return reinterpret_cast<const uint8_t*>(&x); }
};
template<class X> struct BoundType<std::vector<X>>
{
	static constexpr bool isVector () { return true; }
	static constexpr VkDeviceSize size () { return sizeof(X); }
	static VkDeviceSize length (const std::vector<X>& x) { return sizeof(X) * x.size(); }
	static uint8_t* data (std::vector<X>& x) { return reinterpret_cast<uint8_t*>(x.data()); }
	static const uint8_t* data (const std::vector<X>& x) { return reinterpret_cast<const uint8_t*>(x.data()); }
};
} // hidden
template<class X> uint32_t PipelineLayout::joinBindings (VkDescriptorType type, VkShaderStageFlags stages, uint32_t count)
{
	auto index = std::type_index(typeid(typename add_extent<X>::type));
	return joinBindings_(index, hidden::BoundType<X>::size(), hidden::BoundType<X>::isVector(), type, stages, std::max(count, 1u));
}
template<class X> uint32_t PipelineLayout::addBinding (uint32_t elementCount, VkDescriptorType type, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<X>::type));
	return addBinding_(index, hidden::BoundType<X>::size(), hidden::BoundType<X>::isVector(), type, stages,
					   std::max((hidden::BoundType<X>::isVector() ? elementCount : 1u), 1u));
}
template<class Y> uint32_t PipelineLayout::addBindingAsVector (uint32_t elementCount, VkDescriptorType type, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<std::vector<Y>>::type));
	return addBinding_(index, hidden::BoundType<Y>::size(), true, type, stages, std::max(elementCount, 1u));
}
template<class X> uint32_t PipelineLayout::addBindingAsVector (const std::vector<X>& vector, VkDescriptorType type, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<std::vector<X>>::type));
	return addBinding_(index, sizeof(X), true, type, stages, std::max(static_cast<uint32_t>(vector.size()), 1u));
}
template<class Data__> void PipelineLayout::writeBinding (uint32_t binding, const Data__& data)
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	writeBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data));
}
template<class Data__> void PipelineLayout::readBinding (uint32_t binding, Data__& data) const
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	readBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data));
}
template<class X> std::optional<X> PipelineLayout::getBinding (uint32_t binding) const
{
	std::optional<X> result;
	getBinding_(binding, result);
	return result;
}
template<class PC__> ZPipelineLayout PipelineLayout::createPipelineLayout (VkShaderStageFlags pcStageFlags)
{
	const VkPushConstantRange pcr
	{
		pcStageFlags,	// stageFlags
		0,				// offset
		sizeof(PC__)	// size
	};
	return createPipelineLayout_({}, &pcr, std::type_index(typeid(PC__)));
}
template<class PC__> ZPipelineLayout PipelineLayout::createPipelineLayout (ZDescriptorSetLayout dsLayout, VkShaderStageFlags pcStageFlags)
{
	const VkPushConstantRange pcr
	{
		pcStageFlags,	// stageFlags
		0,				// offset
		sizeof(PC__)	// size
	};
	return createPipelineLayout_({dsLayout}, &pcr, std::type_index(typeid(PC__)));
}

} // namespace vtf

#endif // __VTF_PIPELINE_LAYOUT_HPP_INCLUDED__
