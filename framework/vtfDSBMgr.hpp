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
#include "vtfZPushConstants.hpp"

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

class DescriptorSetBindingManager
{
public:
				ZDevice			device;
				ZQueue			queue;
				ZCommandPool	cmdPool;
				DescriptorSetBindingManager	(ZDevice device_);
				DescriptorSetBindingManager	(ZDevice device_, ZQueue queue_);
				DescriptorSetBindingManager	(ZDevice device_, ZCommandPool cmdPool_);
				DescriptorSetBindingManager	(add_cref<DescriptorSetBindingManager>) = delete;
				DescriptorSetBindingManager	(add_rref<DescriptorSetBindingManager>) = delete;

	inline static uint32_t maxBindingCount = 64u;

	/// <summary>
	/// Adds descriptors such as STORAGE_BUFFER or UNIFORM_BUFFER
	/// </summary>
	uint32_t	addBinding	(ZBuffer, VkDescriptorType,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);
	uint32_t	addBinding	(uint32_t suggestedBinding, ZBuffer, VkDescriptorType,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);
	/// <summary>
	/// Adds descriptors such as STORAGE_IMAGE, SAMPLED_IMAGE, INPUT_ATTACHMENT
	/// </summary>
	uint32_t	addBinding	(ZImageView, VkDescriptorType,
							VkImageLayout = VK_IMAGE_LAYOUT_GENERAL, VkShaderStageFlags = VK_SHADER_STAGE_ALL,
							VkDescriptorBindingFlags = 0);
	uint32_t	addBinding	(uint32_t suggestedBinding, ZImageView, VkDescriptorType,
							VkImageLayout = VK_IMAGE_LAYOUT_GENERAL, VkShaderStageFlags = VK_SHADER_STAGE_ALL,
							VkDescriptorBindingFlags = 0);

	uint32_t	addBinding	(ZImageView, ZSampler, VkImageLayout = VK_IMAGE_LAYOUT_GENERAL,
							VkShaderStageFlags= VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);
	uint32_t	addBinding	(uint32_t suggestedBinding, ZImageView, ZSampler,
							VkImageLayout = VK_IMAGE_LAYOUT_GENERAL, VkShaderStageFlags= VK_SHADER_STAGE_ALL,
							VkDescriptorBindingFlags = 0);

	uint32_t	addBinding	(ZSampler, VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);
	uint32_t	addBinding	(uint32_t suggestedBinding, ZSampler, VkShaderStageFlags = VK_SHADER_STAGE_ALL,
							VkDescriptorBindingFlags = 0);

	/// <summary>
	/// Special version to create null-descriptors
	/// </summary>
	uint32_t	addBinding	(std::nullptr_t, VkDescriptorType,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);
	uint32_t	addBinding	(uint32_t suggestedBinding, std::nullptr_t, VkDescriptorType,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);

	/// <summary>
	/// Adds any kind of descriptor includeing MUTABLE_EXT.
	/// It must updated manually via DescriptorSetBindingManager::updateDescriptorSet(...) method.
	/// </summary>
	uint32_t	addBinding	(VkDescriptorType, VkImageLayout = VK_IMAGE_LAYOUT_GENERAL,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL,
							std::initializer_list<VkDescriptorType> mutableTypes = {},
							VkDescriptorBindingFlags = 0u);
	uint32_t	addBinding	(uint32_t suggestedBinding,
							VkDescriptorType, VkImageLayout = VK_IMAGE_LAYOUT_GENERAL,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL,
							std::initializer_list<VkDescriptorType> mutableTypes = {},
							VkDescriptorBindingFlags = 0u);

	uint32_t	addArrayBinding	(ZBuffer, VkDescriptorType, uint32_t elemCount, uint32_t stride,
							VkShaderStageFlags = VK_SHADER_STAGE_ALL, VkDescriptorBindingFlags = 0);

	/**
	 * Allows to write data directly to an associated data buffer.
	 * A type of Data_ is veryfied before read, if doesn't match then throws an exception.
	 * This method is available after createDescriptorSetLayout() is called.
	 * :( This interface should be more flexible :(
	 */
	template<class Data__> VkDeviceSize		writeBinding				(uint32_t binding, Data__ const& data,
																		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL) const;

	template<class Data__> VkDeviceSize		fillBinding					(uint32_t binding, Data__ const& data,
																		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL) const;
	/**
	 * Allows to direct read from associated buffer. A type of Data_ is veryfied before read.
	 * If data is a vector then it must be resized to desired size but only declared elements
	 * will be read, remaining part of the vector will contain undefined values.
	 * This method is available after createDescriptorSetLayout() is called.
	 */
	template<class Data__> VkDeviceSize		readBinding					(uint32_t binding, Data__& data,
																		 VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL) const;
	uint32_t								getBindingElementCount		(uint32_t binding) const;
	bool									containsSamplers			() const;
	/**
	 * Creates descriptor set layout object based on bindings collected
	 * via addBinding*(...) methods. Automatically creates descriptor set
	 * object and bind it to newly created descriptor set layout.
	 * Additionaly performs an update on demand with newly created descriptor set.
	 * Any time you can this descriptor with getDescriptorSet(...) and update it manually.
	 * Each of descritor set has unique identifier of this DescriptorSetBindingManager.
	 */
	uint32_t								getIdentifier				() const { return m_identifier; }
	void									assertDoubledBindings		() const;
	ZDescriptorSetLayout					createDescriptorSetLayout	(bool performUpdateDescriptorSets = true,
																		 VkDescriptorSetLayoutCreateFlags = VkDescriptorSetLayoutCreateFlags(0),
																		 VkDescriptorPoolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
	static ZDescriptorSetLayout				getDescriptorSetLayout		(ZPipelineLayout layout, uint32_t index = 0u);
	static ZDescriptorSet					getDescriptorSet			(ZDescriptorSetLayout dsLayout);
	/**
	 * Updates descriptor set bindings. Descriptor set must match a descriptor
	 * set achieved from this class along with the bindings being updated.
	 */
	void	updateDescriptorSet	(ZDescriptorSet ds, uint32_t binding, ZBuffer,
								std::optional<VkDescriptorType> mutableVariant = {});
	void	updateDescriptorSet	(ZDescriptorSet ds, uint32_t binding, ZSampler sampler);
	// If layout is UNDEFINED then it will be taken from addBinding(imageLayout),
	// if MAX_ENUM then from layout built-in in ZImage, otherwise value of given layout will be used.
	void	updateDescriptorSet	(ZDescriptorSet ds, uint32_t binding, ZImageView view,
								VkImageLayout layout = VK_IMAGE_LAYOUT_MAX_ENUM,
								std::optional<VkDescriptorType> mutableVariant = {});
	void	updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZImageView view,
								ZSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_MAX_ENUM);
	void	updateDescriptorSet	(ZDescriptorSet	descriptorSet);

	ZPipelineLayout				 createPipelineLayout ();
	ZPipelineLayout				 createPipelineLayout (add_cref<ZPushConstants> pushConstants);
	ZPipelineLayout				 createPipelineLayout (std::initializer_list<ZDescriptorSetLayout>, add_cref<ZPushConstants>);
	template<class... PC__>	auto createPipelineLayout (const ZPushRange<PC__>&...) -> ZPipelineLayout;
	template<class... PC__>	auto createPipelineLayout (std::initializer_list<ZDescriptorSetLayout>, const ZPushRange<PC__>&...) -> ZPipelineLayout;

	ZBuffer						 createDescriptorBuffer (ZDescriptorSetLayout dsLayout);

protected:	
	typedef struct VkDescriptorSetLayoutBindingAndType : VkDescriptorSetLayoutBinding
	{
		VkDescriptorSetLayoutBindingAndType ()
			: index			(typeid(void))
			, isNull		(false)
			, imageLayout	(VK_IMAGE_LAYOUT_UNDEFINED)
			, bindingFlags	(0)
			, mutableTypes	()
			, mutableIndex	(INVALID_UINT32)
			, stride		(0)
		{}
		std::type_index					index;
		bool							isNull;
		VkImageLayout					imageLayout;
		VkDescriptorBindingFlags		bindingFlags;
		std::vector<VkDescriptorType>	mutableTypes;
		uint32_t						mutableIndex;
		uint32_t						stride; // TODO: reuse this field for other types than buffer
		ZBuffer							buffer;
		ZImageView						view;
		ZSampler						sampler;
	} ExtBinding;
	typedef std::vector<ExtBinding>	ExtBindings;

	uint32_t	addBinding_	(uint32_t, std::type_index, ZBuffer, ZImageView, ZSampler,
							VkDescriptorType, bool, VkShaderStageFlags, VkDescriptorBindingFlags,
							VkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							std::initializer_list<VkDescriptorType> mutableTypes = {},
							uint32_t descriptorCount = 1u, uint32_t stride = 0);
		  ExtBinding&	verifyGetExtBinding		(uint32_t binding);
	const ExtBinding&	verifyGetExtBinding		(uint32_t binding) const;
	const ExtBinding&	verifyGetExtBinding		(std::type_index index, uint32_t binding) const;

	struct VectorWrapper
	{
		virtual void resize (uint32_t size) const = 0;
		virtual add_ptr<uint8_t> data () const = 0;
	};
	template<class ResizeLambda, class DataLambda>
	struct VectorWrapperImpl : VectorWrapper
	{
		DataLambda m_dataLambda;
		ResizeLambda m_resizeLambda;
		VectorWrapperImpl (ResizeLambda resizeLambda, DataLambda dataLambda)
			: m_dataLambda(dataLambda), m_resizeLambda(resizeLambda) {}
		virtual void resize (uint32_t size) const override {
			m_resizeLambda(size);
		}
		virtual add_ptr<uint8_t> data() const override {
			return reinterpret_cast<add_ptr<uint8_t>>(m_dataLambda());
		}
	};

	VkDeviceSize		writeResource_			(ZImageView view, std::type_index dataIndex, std::type_index resIndex,
												add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const;
	VkDeviceSize		writeResource_			(ZBuffer buffer, std::type_index dataIndex, std::type_index resIndex,
												add_cptr<uint8_t> data, VkDeviceSize bytes) const;
	VkDeviceSize		writeBinding_			(std::type_index index, uint32_t binding,
												add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const;
	VkDeviceSize		fillBinding_			(std::type_index index, uint32_t binding,
												add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const;
	VkDeviceSize		readResourceUnchecked_	(ZBuffer buffer, std::type_index dataIndex,
												 add_ptr<uint8_t> data, uint32_t elementSize, bool isVector, add_cptr<VectorWrapper> pww) const;
	VkDeviceSize		readResource_			(ZBuffer buffer, std::type_index dataIndex, std::type_index resIndex,
												 add_ptr<uint8_t> data, uint32_t elementSize, bool isVector, add_cptr<VectorWrapper> pww) const;
	VkDeviceSize		readResource_			(ZImageView view, std::type_index dataIndex, std::type_index resIndex,
												 add_ptr<uint8_t> data, uint32_t elementSize, bool isVector, VkImageLayout finalLayout,
												 add_cptr<VectorWrapper> pww) const;
	VkDeviceSize		readBinding_			(std::type_index index, uint32_t binding,
												 add_ptr<uint8_t> data, uint32_t elementSize,
												 bool isVector, VkImageLayout finalLayout,
												 add_cptr<VectorWrapper> pww) const;

	ZPipelineLayout		createPipelineLayout_	(add_cref<ZPushConstants> pushConstants,
												 std::initializer_list<ZDescriptorSetLayout> dsLayouts);
private:
	ExtBindings											m_extbindings;
	uint32_t											m_identifier;
	static uint32_t										m_counter;
};
// just most friendly name and shorter :)
using LayoutManager = DescriptorSetBindingManager;
namespace hidden
{
template<class X> struct BoundType
{
	static constexpr bool isVector () { return false; }
	static constexpr uint32_t elementSize () { return uint32_t(sizeof(X)); }
	static uint32_t length (X const&) { return uint32_t(sizeof(X)); }
	static add_ptr<uint8_t> data (X& x) { return reinterpret_cast<add_ptr<uint8_t>>(&x); }
	static add_cptr<uint8_t> data (X const& x) { return reinterpret_cast<add_cptr<uint8_t>>(&x); }
};
template<class X> struct BoundType<std::vector<X>>
{
	static constexpr bool isVector () { return true; }
	static constexpr uint32_t elementSize() { return uint32_t(sizeof(X)); }
	static uint32_t length (std::vector<X> const& x) { return uint32_t(x.size() * sizeof(X)); }
	static add_ptr<uint8_t> data (std::vector<X>& x) { return reinterpret_cast<add_ptr<uint8_t>>(x.data()); }
	static add_cptr<uint8_t> data (std::vector<X> const& x) { return reinterpret_cast<add_cptr<uint8_t>>(x.data()); }
};
} // namespace hidden

template<class Data__> VkDeviceSize DescriptorSetBindingManager::writeBinding (
	uint32_t binding, Data__ const& data, VkImageLayout finalLayout) const
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	return writeBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data), finalLayout);
}
template<class Data__> VkDeviceSize DescriptorSetBindingManager::fillBinding (
	uint32_t binding, Data__ const& data, VkImageLayout finalLayout) const
{
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	return fillBinding_(index, binding, hidden::BoundType<Data__>::data(data), hidden::BoundType<Data__>::length(data), finalLayout);
}
template<class Data__> VkDeviceSize DescriptorSetBindingManager::readBinding (
	uint32_t binding, Data__& data, VkImageLayout finalLayout) const
{
	add_ptr<VectorWrapper> pww = nullptr;
	auto index = std::type_index(typeid(typename add_extent<Data__>::type));
	constexpr bool isVactor = hidden::BoundType<Data__>::isVector();
	if constexpr (isVactor)
	{
		auto onResize = [&](uint32_t size) -> void { data.resize(size); };
		auto onData = [&] { return data.data(); };
		VectorWrapperImpl ww(onResize, onData);
		pww = &ww;
	}
	return readBinding_(index, binding, hidden::BoundType<Data__>::data(data),
						hidden::BoundType<Data__>::elementSize(), isVactor, finalLayout, pww);
}
template<class... PC__> ZPipelineLayout
DescriptorSetBindingManager::createPipelineLayout (const ZPushRange<PC__>&... ranges)
{
	return createPipelineLayout_(ZPushConstants(ranges...), {});
}

template<class... PC__> ZPipelineLayout
DescriptorSetBindingManager::createPipelineLayout (std::initializer_list<ZDescriptorSetLayout> dsLayouts, const ZPushRange<PC__>&... ranges)
{
	return createPipelineLayout_(ZPushConstants(ranges...), dsLayouts);
}

} // namespace vtf

#endif // __VTF_PIPELINE_LAYOUT_HPP_INCLUDED__
