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
	ZSampler		sampler;
	ZImageView		view;
	ZImage			image;
	VkImageLayout	layout;
	DescriptorImageInfo (ZSampler		sampler_,
						 ZImageView		view_,
						 ZImage			image_,
						 VkImageLayout	layout_)
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

/**
 * @brief The LayoutManager class that simplifies pipeline layout creation.
 *
 * Casual scenario:
 * ================
 * extern ZDevice		device;
 * ZBuffer				buffer				= createBuffer(..., ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BIT));
 * ZImage				image				= createImage(..., ZImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT));
 * ZImageView			view				= createImageView(image);
 * LayoutManager		lm					(device);
 * struct				SSBO				{ uint32_t a,b,c; } ssbo;
 * uint32_t				bufferBinding		= lm.addBinding<SSBO>();
 * uint32_t				textureBinding		= lm.addBinding(view, ZSampler());
 * ZDescriptorSetLayout	descriptorSetLayout	= lm.createDescriptorSetLayout();
 * struct PushConstant						{ Vec4 data; };
 * ZPipelineLayout		pipelineLayout		= lm.createPipelineLayout<PushContant>(descriptorSetLayout);
 * ZPipeline			pipeline			= create(Compute|Graphics)Pipeline(...);
 * //
 * // Access to the SSBO buffer
 * ZBuffer				testBuffer			= std::get<DescriptorBufferInfo>(lm.getDescriptorInfo(bufferBinding)).buffer;
 */
class LayoutManager
{
public:
											ZDevice						device;
											LayoutManager				(ZDevice device);
											LayoutManager				(add_cref<LayoutManager>) = delete;
											LayoutManager				(add_rref<LayoutManager>) = delete;
	/**
	 * Creates single buffer and concatenates all chunks into it
	 * that have the same descriptor type and buffer usage.
	 */
	template<class X> uint32_t				joinBindings				(VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
																		 VkShaderStageFlags stages = VK_SHADER_STAGE_ALL, uint32_t count = 1);
	/**
	 * Creates separate binding, usefull for vectors SSBO.
	 * In the case where X is a std::vector<Y> an elementCount is the element count of that vector.
	 * In the case that X is standalone type (e.g. user structure) the elementCount param is ignored.
	 */
	template<class VecOrElemT> uint32_t		addBinding					(VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uint32_t elementCount = 1, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	/**
	 * Basically does the same things like above addBinding().
	 * The one difference is that it automatically treat Y as std::vector<Y>
	 */
	template<class ElemType> uint32_t		addBindingAsVector			(VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uint32_t elementCount = 1, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	template<class ElemType> uint32_t		addBindingAsVector			(const std::vector<ElemType>& vector, VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	/**
	 * Creates a descriptor set bind depending on parameters.
	 * If both view and sampler have handles then descriptor be VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER.
	 * If view has a handle and sampler don't then descriptor be VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE.
	 * If an image connected to the view has VK_IMAGE_USAGE_STORAGE_BIT then VK_DESCRIPTOR_TYPE_STORAGE_IMAGE.
	 * If view has no handle but sampler has then descriptor be VK_DESCRIPTOR_TYPE_SAMPLER.
	 * Anyway you can specify descriptor type exactly by type parameter.
	 * In the case of an image imageLayout parameter tells about a way how the descriptor will use that image.
	 */
	uint32_t								addBinding					(ZImageView view, ZSampler sampler,
																		 VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL,
																		 VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM,
																		 VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	/**
	 * Creates separate 'empty' binding for storage image, sampled image and sampled image with sampler.
	 * Remember to update this binding with updateDescriptorSet(...) with proper view and/or sampler.
	 */
	uint32_t								addBinding					(VkDescriptorType type,
																		 VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL,
																		 VkShaderStageFlags stages = VK_SHADER_STAGE_ALL);
	template<class X> std::optional<X>		getBinding					(uint32_t binding) const;
	/**
	 * Allows to write data directly to an associated data buffer.
	 * A type of Data_ is veryfied before read, if doesn't match then throws an exception.
	 * This method is available after createDescriptorSetLayout() is called.
	 * :( This interface should be more flexible :(
	 */
	template<class Data__> void				writeBinding				(uint32_t binding, const Data__& data);
	template<class VecElem__> void			writeBinding				(uint32_t binding, const VecElem__& data, uint32_t index);
	void									fillBinding					(uint32_t binding, uint32_t value = 0u);
	/**
	 * Allows to direct read from associated buffer. A type of Data_ is veryfied before read.
	 * If data is a vector then it must be resized to desired size but only declared elements
	 * will be read, remaining part of the vector will contain undefined values.
	 * This method is available after createDescriptorSetLayout() is called.
	 */
	template<class Data__> void				readBinding					(uint32_t binding, Data__& data) const;
	uint32_t								getBindingElementCount		(uint32_t binding) const;
	VarDescriptorInfo						getDescriptorInfo			(uint32_t binding) const;
	/**
	 * Creates descriptor set layout object based on bindings collected
	 * via addBinding*(...) methods. Automatically creates descriptor set
	 * object and bind it to newly created descriptor set layout.
	 * Additionaly performs an update on newly created descriptor set.
	 * Any time you can this descriptor with getDescriptorSet(...) and
	 * update it manually.
	 */
	ZDescriptorSetLayout					createDescriptorSetLayout	(bool performUpdateDescriptorSets = true);
	static ZDescriptorSetLayout				getDescriptorSetLayout		(ZPipelineLayout layout, uint32_t index = 0u);
	static ZDescriptorSet					getDescriptorSet			(ZDescriptorSetLayout dsLayout);
	/**
	 * Updates descriptor set bindings. Descriptor set must match a descriptor
	 * set achieved from this class along with the bindings being updated. Or
	 * it might be null-handle descriptor, then only internal data is updated.
	 * If the binding is view+sampler type only one of them is updated in the
	 * case single view/sampler parameter, otherwise both if given.
	 */
	void									updateDescriptorSet			(ZDescriptorSet ds, uint32_t binding, ZImageView view);
	void									updateDescriptorSet			(ZDescriptorSet ds, uint32_t binding, ZSampler sampler);
	void									updateDescriptorSet			(ZDescriptorSet ds, uint32_t binding, ZImageView view, ZSampler sampler);
	void									updateDescriptorSet			(ZDescriptorSet ds, uint32_t binding, ZBuffer buffer);
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
		VkImageLayout	imageLayout;
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
	void									getBinding_					(uint32_t binding, std::optional<ZBuffer>&) const;
	void									getBinding_					(uint32_t binding, std::optional<ZImageView>&) const;
	void									getBinding_					(uint32_t binding, std::optional<ZSampler>&) const;
	void									getBinding_					(uint32_t binding, std::optional<std::pair<ZImageView,ZSampler>>&) const;
	VkDeviceSize							getDescriptorAlignment		(const VkDescriptorType type) const;
	void									updateBuffersOffsets		();
	void									recreateUpdateBuffers		(std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers, bool performUpdateDescriptorSets);
	void									updateDescriptorSet_		(ZDescriptorSet	descriptorSet,
																		 std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers) const;
	ZPipelineLayout							createPipelineLayout_		(std::initializer_list<ZDescriptorSetLayout> descriptorSetLayouts,
																		 const VkPushConstantRange* pPushConstantRanges, type_index_with_default typeOfPushConstant);

private:
	ExtBindings													m_extbindings;
	std::vector<ZImageView>										m_views;
	std::vector<ZSampler>										m_samplers;
	std::vector<std::pair<ZImageView,ZSampler>>					m_viewsAndSamplers;
	//std::vector<ZBuffer>										m_texelBuffers;
	std::map<std::pair<VkDescriptorType, int>, ZBuffer>			m_buffers;
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
template<class X> uint32_t LayoutManager::joinBindings (VkDescriptorType type, VkShaderStageFlags stages, uint32_t count)
{
	auto index = std::type_index(typeid(typename add_extent<X>::type));
	return joinBindings_(index, hidden::BoundType<X>::size(), hidden::BoundType<X>::isVector(), type, stages, std::max(count, 1u));
}
template<class VecOrElemT> uint32_t LayoutManager::addBinding (VkDescriptorType type, uint32_t elementCount, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<VecOrElemT>::type));
	return addBinding_(index, hidden::BoundType<VecOrElemT>::size(), hidden::BoundType<VecOrElemT>::isVector(),
					   type, stages, std::max((hidden::BoundType<VecOrElemT>::isVector() ? elementCount : 1u), 1u));
}
template<class Y> uint32_t LayoutManager::addBindingAsVector (VkDescriptorType type, uint32_t elementCount, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<std::vector<Y>>::type));
	return addBinding_(index, hidden::BoundType<Y>::size(), true, type, stages, std::max(elementCount, 1u));
}
template<class X> uint32_t LayoutManager::addBindingAsVector (const std::vector<X>& vector, VkDescriptorType type, VkShaderStageFlags stages)
{
	auto index = std::type_index(typeid(typename add_extent<std::vector<X>>::type));
	return addBinding_(index, sizeof(X), true, type, stages, std::max(static_cast<uint32_t>(vector.size()), 1u));
}
template<class Data__> void LayoutManager::writeBinding (uint32_t binding, const Data__& data)
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	writeBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data));
}
template<class VecElem__> void LayoutManager::writeBinding (uint32_t binding, const VecElem__& data, uint32_t index)
{
	UNREF(binding);
	UNREF(data);
	UNREF(index);
	auto tindex = std::type_index(typeid(typename add_extent<std::vector<VecElem__>>::type));
	UNREF(tindex);
}
template<class Data__> void LayoutManager::readBinding (uint32_t binding, Data__& data) const
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	readBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data));
}
template<class X> std::optional<X> LayoutManager::getBinding (uint32_t binding) const
{
	std::optional<X> result;
	getBinding_(binding, result);
	return result;
}
template<class PC__> ZPipelineLayout LayoutManager::createPipelineLayout (VkShaderStageFlags pcStageFlags)
{
	const VkPushConstantRange pcr
	{
		pcStageFlags,	// stageFlags
		0,				// offset
		sizeof(PC__)	// size
	};
	return createPipelineLayout_({}, &pcr, type_index_with_default(typeid(PC__)));
}
template<class PC__> ZPipelineLayout LayoutManager::createPipelineLayout (ZDescriptorSetLayout dsLayout, VkShaderStageFlags pcStageFlags)
{
	const VkPushConstantRange pcr
	{
		pcStageFlags,	// stageFlags
		0,				// offset
		sizeof(PC__)	// size
	};
	return createPipelineLayout_({dsLayout}, &pcr, type_index_with_default(typeid(PC__)));
}

} // namespace vtf

#endif // __VTF_PIPELINE_LAYOUT_HPP_INCLUDED__
