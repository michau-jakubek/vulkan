#include "vtfDSBMgr.hpp"
#include "vtfZUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfCUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include <vulkan/vulkan_to_string.hpp>
#include <iostream>

[[maybe_unused]] static std::pair<VkDescriptorType, VkBufferUsageFlagBits>
const DescriptorTypeToBufferUsage[]
{
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
};

namespace vtf
{

uint32_t DescriptorSetBindingManager::m_counter;

DescriptorSetBindingManager::DescriptorSetBindingManager (ZDevice device_)
	: device		(device_)
	, queue			()
	, cmdPool		()
	, m_extbindings	()
	, m_identifier	(m_counter++)
{
	ASSERTMSG(device.has_handle(), "Device must have a handle");
	m_extbindings.reserve(maxBindingCount);
}

DescriptorSetBindingManager::DescriptorSetBindingManager (ZDevice device_, ZQueue queue_)
	: device		(device_)
	, queue			(queue_)
	, cmdPool		(device && queue ? createCommandPool(device, queue) : ZCommandPool())
	, m_extbindings	()
	, m_identifier	(m_counter++)
{
	ASSERTMSG(device.has_handle(), "Device must have a handle");
	ASSERTMSG(queue.has_handle(), "Queue must have a handle");
	m_extbindings.reserve(maxBindingCount);
}

DescriptorSetBindingManager::DescriptorSetBindingManager(ZDevice device_, ZCommandPool cmdPool_)
	: device		(device_)
	, queue			(cmdPool_.getParam<ZQueue>())
	, cmdPool		(cmdPool_)
	, m_extbindings	()
	, m_identifier	(m_counter++)
{
	m_extbindings.reserve(maxBindingCount);
	ASSERTMSG(device.has_handle(), "Device must have a handle");
	ASSERTMSG(cmdPool.has_handle(), "ZCommandPool must have a handle");
	ASSERTMSG(queue.has_handle(), "Queue must have a handle");
}

// TODO: move below to template utils file
template<template<class, class...> class Container, class value_type, class... Aux>
uint32_t index_of (Container<value_type, Aux...> const& cntr, value_type const& value)
{
	if (auto first = std::find(cntr.begin(), cntr.end(), value); cntr.end() != first)
		return static_cast<uint32_t>(std::distance(cntr.begin(), first));
	return INVALID_UINT32;
}

template<class X> void vector_from_initializer_list (std::initializer_list<X> src, add_ref<std::vector<X>> dst)
{
	dst.clear();
	dst.reserve(src.size());
	for (add_cref<X> x : src)
		dst.emplace_back(x);
}
// TODO merge together vector_from_initializer_list and vector_from_initializer_unique_list 
template<class X> void vector_from_initializer_unique_list (std::initializer_list<X> src, add_ref<std::vector<X>> dst)
{
	std::set<X> unique_list;
	for (add_cref<X> x : src)
		unique_list.insert(x);
	dst.clear();
	dst.reserve(unique_list.size());
	for (add_cref<X> x : unique_list)
		dst.emplace_back(x);
}

bool descriptorTypeOnVector (const VkDescriptorType type, add_cref<std::vector<VkDescriptorType>> list)
{
	ASSERTMSG(list.size(), "Mutable type list must not be empty");
	return list.end() != std::find(list.begin(), list.end(), type);
}
bool descriptorTypeOnList (const VkDescriptorType type, std::initializer_list<VkDescriptorType> list)
{
	ASSERTMSG(list.size(), "Mutable type list must not be empty");
	return list.end() != std::find(list.begin(), list.end(), type);
}

uint32_t DescriptorSetBindingManager::addBinding_ (uint32_t suggestedBinding,
	std::type_index index, ZBuffer buffer, ZImageView view, ZSampler sampler,
	VkDescriptorType type, bool isNull, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags,
	VkImageLayout imageLayout, std::initializer_list<VkDescriptorType> mutableTypes,
	uint32_t descriptorCount, uint32_t stride)
{
	if (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT == type)
	{
		stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	VkDescriptorSetLayoutBindingAndType b{};
	const uint32_t binding = (INVALID_UINT32 == suggestedBinding)
		? data_count(m_extbindings) : suggestedBinding;
	b.binding				= binding;
	b.descriptorCount		= descriptorCount;
	b.descriptorType		= type;
	b.pImmutableSamplers	= nullptr;
	b.stageFlags			= stageFlags;
	b.index					= index;
	b.isNull				= isNull;
	b.imageLayout			= imageLayout;
	b.bindingFlags			= bindingFlags;
	ASSERTMSG(VK_DESCRIPTOR_TYPE_MUTABLE_EXT != type || mutableTypes.size(),
				"If VK_DESCRIPTOR_TYPE_MUTABLE_EXT then mutableTypes must not be empty");
	vector_from_initializer_unique_list(mutableTypes, b.mutableTypes);
	b.mutableIndex			= INVALID_UINT32;
	b.stride	= stride;
	b.buffer	= buffer;
	b.view		= view;
	b.sampler	= sampler;
	m_extbindings.emplace_back(b);
	return binding;
}

uint32_t DescriptorSetBindingManager::addBinding (
	ZBuffer buffer, VkDescriptorType type,
	VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, buffer, type, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (
	uint32_t suggestedBinding, ZBuffer buffer, VkDescriptorType type,
	VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	ASSERTMSG(buffer.has_handle(), "Buffer must have a handle");
	auto index = std::type_index(typeid(typename add_extent<ZBuffer>::type));
	return addBinding_(suggestedBinding, index, buffer, {/*view*/}, {/*sampler*/},
						type, false, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addArrayBinding(
	ZBuffer buffer, VkDescriptorType type, uint32_t elemCount, uint32_t stride,
	VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	const auto minStorageBufferOffsetAlignment =
		deviceGetPhysicalLimits(buffer.getParam<ZDevice>()).minStorageBufferOffsetAlignment;

	ASSERTMSG(buffer.has_handle(), "Buffer must have a handle");
	ASSERTMSG(stride && ((stride % minStorageBufferOffsetAlignment) == 0u),
		"Stride (", stride, ") is not multiplication of "
		"minStorageBufferOffsetAlignment (", minStorageBufferOffsetAlignment, ")");
	const VkDeviceSize requiredSize = elemCount * stride;
	const VkDeviceSize bufferSize = bufferGetSize(buffer);
	ASSERTMSG(bufferSize >= requiredSize, "Insufficient buffer memory: "
		"bufferSize (", bufferSize, ") < elemCount(", elemCount, ") * stride (", stride, ") "
		" = ", requiredSize);
	auto index = std::type_index(typeid(typename add_extent<ZBuffer>::type));
	return addBinding_(INVALID_UINT32, index, buffer, {/*view*/ }, {/*sampler*/ },
						type, false, stageFlags, bindingFlags, VK_IMAGE_LAYOUT_UNDEFINED, {},
						elemCount, stride);
}

uint32_t DescriptorSetBindingManager::addBinding (
	ZImageView view, VkDescriptorType type,
	VkImageLayout imageLayout, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, view, type, imageLayout, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (
	uint32_t suggestedBinding, ZImageView view, VkDescriptorType type,
	VkImageLayout imageLayout, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	ASSERTMSG(view.has_handle(), "View must have a handle");
	auto index = std::type_index(typeid(typename add_extent<ZImageView>::type));
	return addBinding_(suggestedBinding, index, {/*buffer*/}, view, {/*sampler*/},
						type, false, stageFlags, bindingFlags, imageLayout);
}

uint32_t DescriptorSetBindingManager::addBinding (
	ZSampler sampler, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, sampler, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (uint32_t suggestedBinding,
	ZSampler sampler, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	ASSERTMSG(sampler.has_handle(), "Sampler must have a handle");
	auto index = std::type_index(typeid(typename add_extent<ZSampler>::type));
	return addBinding_(suggestedBinding, index, {/*buffer*/}, {/*view*/}, sampler,
						VK_DESCRIPTOR_TYPE_SAMPLER, false, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (
	ZImageView view, ZSampler sampler,
	VkImageLayout imageLayout, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, view, sampler, imageLayout, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (
	uint32_t suggestedBinding, ZImageView view, ZSampler sampler,
	VkImageLayout imageLayout, VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	ASSERTMSG(view.has_handle() && sampler.has_handle(), "Both view and sampler must have a handle");
	auto index = std::type_index(typeid(typename add_extent<std::pair<ZImageView, ZSampler>>::type));
	return addBinding_(suggestedBinding, index, {/*buffer*/}, view, sampler,
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, false, stageFlags, bindingFlags, imageLayout);
}

uint32_t DescriptorSetBindingManager::addBinding (
	VkDescriptorType type, VkImageLayout imageLayout, VkShaderStageFlags stageFlags,
	std::initializer_list<VkDescriptorType> mutableTypes, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, type, imageLayout, stageFlags, mutableTypes, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (uint32_t suggestedBinding,
	VkDescriptorType type, VkImageLayout imageLayout, VkShaderStageFlags stageFlags,
	std::initializer_list<VkDescriptorType> mutableTypes, VkDescriptorBindingFlags bindingFlags)
{
	auto index = std::type_index(typeid(typename add_extent<VkDescriptorType>::type));
	return addBinding_(suggestedBinding, index, {/*buffer*/}, {/*view*/}, {/*sampler*/},
						type, false, stageFlags, bindingFlags, imageLayout, mutableTypes);
}

uint32_t DescriptorSetBindingManager::addBinding(
	std::nullptr_t nullValue, VkDescriptorType type,
	VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	return addBinding(INVALID_UINT32, nullValue, type, stageFlags, bindingFlags);
}

uint32_t DescriptorSetBindingManager::addBinding (
	uint32_t suggestedBinding, std::nullptr_t, VkDescriptorType type,
	VkShaderStageFlags stageFlags, VkDescriptorBindingFlags bindingFlags)
{
	auto index = std::type_index(typeid(typename add_extent<VkDescriptorType>::type));
	return addBinding_(suggestedBinding, index, {/*buffer*/}, {/*view*/}, {/*sampler*/},
						type, true, stageFlags, bindingFlags);
}

ZDescriptorSetLayout DescriptorSetBindingManager::getDescriptorSetLayout (ZPipelineLayout layout, uint32_t index)
{
	add_ref<std::vector<ZDescriptorSetLayout>> dsLayouts
			= layout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	return dsLayouts[index];
}

void DescriptorSetBindingManager::assertDoubledBindings () const
{
	for (auto begin = m_extbindings.begin(), b = begin; b != m_extbindings.end(); ++b)
		for (auto c = std::next(b); c != m_extbindings.end(); ++c)
		{
			ASSERTMSG(b->binding != c->binding, "Doubled binding (", b->binding, ')');
		}
}

ZDescriptorSetLayout DescriptorSetBindingManager::createDescriptorSetLayout (
	bool								performUpdateDescriptorSets,
	VkDescriptorSetLayoutCreateFlags	layoutCreateFlags,
	VkDescriptorPoolCreateFlags			poolCreateFlags)
{
	assertDoubledBindings();

	VkAllocationCallbacksPtr					callbacks			= device.getParam<VkAllocationCallbacksPtr>();
	ZDescriptorPool								descriptorPool		(VK_NULL_HANDLE, device, callbacks);
	ZDescriptorSetLayout						descriptorSetLayout	(VK_NULL_HANDLE, device, callbacks, layoutCreateFlags, ZDescriptorSet(), getIdentifier());
	std::vector<VkDescriptorSetLayoutBinding>	bindings			(m_extbindings.size());
	const bool									descriptorBuffer	= layoutCreateFlags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	std::vector<VkDescriptorBindingFlags>		bindingsFlags		(m_extbindings.size());

	std::vector<VkMutableDescriptorTypeListEXT> mutableBindingLists	(m_extbindings.size());
	uint32_t									mutableBindingCount	= 0u;

	if (descriptorBuffer)
	{
		for (auto begin = m_extbindings.begin(), b = begin; b != m_extbindings.end(); ++b)
		{
			const auto ii = make_unsigned(std::distance(begin, b));
			bindings[ii] = *b;
			bindingsFlags[ii] = b->bindingFlags;
		}
	}
	else
	// build descriptor pool
	{
		std::vector<VkDescriptorPoolSize>		poolSizes;
		std::map<VkDescriptorType, uint32_t>	typesNsizes;

		for (uint32_t ii = 0u; ii < data_count(m_extbindings); ++ii)
		{
			add_cref<ExtBinding> b = m_extbindings[ii];
			typesNsizes[b.descriptorType] += b.descriptorCount;
			bindings[ii] = b;
			bindingsFlags[ii] = b.bindingFlags;

			mutableBindingCount += (b.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT);
			mutableBindingLists[ii].descriptorTypeCount = data_count(b.mutableTypes);
			mutableBindingLists[ii].pDescriptorTypes = data_or_null(b.mutableTypes);
		}

		for (const auto& typeNsize : typesNsizes)
			poolSizes.push_back({ typeNsize.first, typeNsize.second });

		VkDescriptorPoolCreateInfo		poolCreateInfo = makeVkStruct();
		poolCreateInfo.flags = poolCreateFlags;
		poolCreateInfo.maxSets = 1u;
		poolCreateInfo.poolSizeCount = data_count(poolSizes);
		poolCreateInfo.pPoolSizes = data_or_null(poolSizes);
		VKASSERTMSG(vkCreateDescriptorPool(*device, &poolCreateInfo, callbacks,
			descriptorPool.setter()), "Unable to create descriptor pool");
	}

	void_ptr descriptorSetLayoutPNext = nullptr;
	add_ptr<void_ptr> pDescriptorSetLayoutPNext = &descriptorSetLayoutPNext;

	const bool anyNonZeroBindingFlag = std::any_of(bindingsFlags.begin(), bindingsFlags.end(),
										[](add_cref<VkDescriptorBindingFlags> bindingFlag) { return bindingFlag != 0u; });
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI = makeVkStruct();
	if (anyNonZeroBindingFlag)
	{
		bindingFlagsCI.bindingCount = data_count(m_extbindings);
		bindingFlagsCI.pBindingFlags = bindingsFlags.data();
		bindingFlagsCI.pNext = *pDescriptorSetLayoutPNext;
		*pDescriptorSetLayoutPNext = &bindingFlagsCI;
	}

	VkMutableDescriptorTypeCreateInfoEXT mutableTypesCI = makeVkStruct();
	if (mutableBindingCount)
	{
		mutableTypesCI.pMutableDescriptorTypeLists = data_or_null(mutableBindingLists);
		mutableTypesCI.mutableDescriptorTypeListCount = data_count(mutableBindingLists);
		mutableTypesCI.pNext = *pDescriptorSetLayoutPNext;
		*pDescriptorSetLayoutPNext = &mutableTypesCI;
	}

	VkDescriptorSetLayoutCreateInfo		setLayoutCreateInfo = makeVkStruct(*pDescriptorSetLayoutPNext);
	setLayoutCreateInfo.flags			= layoutCreateFlags;
	setLayoutCreateInfo.bindingCount	= data_count(bindings);
	setLayoutCreateInfo.pBindings		= data_or_null(bindings);
	VKASSERTMSG(vkCreateDescriptorSetLayout(*device, &setLayoutCreateInfo, callbacks,
											descriptorSetLayout.setter()), "Failed to create descriptor set layout");

	if (false == descriptorBuffer)
	{
		ZDescriptorSet					descriptorSet(VK_NULL_HANDLE, device, descriptorPool);
		VkDescriptorSetAllocateInfo		allocInfo = makeVkStruct();
		allocInfo.descriptorPool		= *descriptorPool;
		allocInfo.descriptorSetCount	= 1u;
		allocInfo.pSetLayouts			= descriptorSetLayout.ptr();
		VKASSERTMSG(vkAllocateDescriptorSets(*device, &allocInfo,
			descriptorSet.setter()), "Failed to allocate descriptor set");

		if (performUpdateDescriptorSets)
		{
			updateDescriptorSet(descriptorSet);
		}

		descriptorSetLayout.setParam<ZDescriptorSet>(descriptorSet);
	}

	return descriptorSetLayout;
}

void DescriptorSetBindingManager::updateDescriptorSet (
	ZDescriptorSet ds, uint32_t binding, ZBuffer buffer,
	std::optional<VkDescriptorType> mutableVariant)
{
	add_ref<ExtBinding> b = verifyGetExtBinding(binding);
	ASSERTMSG(VK_DESCRIPTOR_TYPE_MUTABLE_EXT != b.descriptorType
		? descriptorTypeOnList(b.descriptorType, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER })
		: (descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, b.mutableTypes)
			|| descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, b.mutableTypes)), "???");
	ASSERTMSG(buffer.has_handle(), "Buffer must have a handle");
	ASSERTMSG(false == bool(mutableVariant) || descriptorTypeOnVector(*mutableVariant, b.mutableTypes), "???");

	std::vector<VkDescriptorBufferInfo>	bufferInfos(b.descriptorCount);

	VkWriteDescriptorSet	writeParams = makeVkStruct();
	writeParams.dstSet			= *ds;
	writeParams.dstBinding		= b.binding;
	writeParams.dstArrayElement	= 0u;
	writeParams.descriptorCount = b.descriptorCount;
	writeParams.descriptorType	= b.descriptorType;
	writeParams.pBufferInfo		= bufferInfos.data();

	if (VK_DESCRIPTOR_TYPE_MUTABLE_EXT == b.descriptorType)
	{
		b.buffer = buffer;
		ASSERTMSG(mutableVariant, "For VK_DESCRIPTOR_TYPE_MUTABLE_EXT mutableVariant must not be empty");
		b.mutableIndex = index_of(b.mutableTypes, *mutableVariant);
		writeParams.descriptorType = *mutableVariant;
	}

	VkDeviceSize arrayElementSize = b.descriptorCount == 1u ? bufferGetSize(buffer) : b.stride;

	for (uint32_t arrayElement = 0u; arrayElement < b.descriptorCount; ++arrayElement)
	{
		bufferInfos[arrayElement].buffer = b.isNull ? VK_NULL_HANDLE : *buffer;
		bufferInfos[arrayElement].offset = b.isNull ? 0u : (arrayElement * arrayElementSize);
		bufferInfos[arrayElement].range  = b.isNull ? 0u : arrayElementSize;
		if (b.descriptorCount > 1u && arrayElement == (b.descriptorCount - 1u) && (false == b.isNull))
		{
			bufferInfos[arrayElement].range += (bufferGetSize(buffer) % b.stride);
		}
	}

	vkUpdateDescriptorSets(*device,
		1u,				// descriptorWriteCount
		&writeParams,	// pDescriptorWrites
		0u,				// copyCount,
		nullptr			// copyParams
	);
}
void DescriptorSetBindingManager::updateDescriptorSet (
	ZDescriptorSet ds, uint32_t binding, ZImageView view,
	VkImageLayout imageLayout, std::optional<VkDescriptorType> mutableVariant)
{
	add_ref<ExtBinding> b = verifyGetExtBinding(binding);
	ASSERTMSG(VK_DESCRIPTOR_TYPE_MUTABLE_EXT != b.descriptorType
		? descriptorTypeOnList(b.descriptorType,
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT })
		: (descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, b.mutableTypes)
			|| descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, b.mutableTypes)
			|| descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, b.mutableTypes)), "???");
	ASSERTMSG(view.has_handle(), "View must have a handle");

	VkDescriptorImageInfo	imageInfo{};
	imageInfo.imageLayout	= (VK_IMAGE_LAYOUT_UNDEFINED == imageLayout)
								? b.imageLayout
								: (VK_IMAGE_LAYOUT_MAX_ENUM == imageLayout)
									? imageGetLayout(imageViewGetImage(view))
									: imageLayout;
	imageInfo.imageView		= b.isNull ? VK_NULL_HANDLE : *view;
	imageInfo.sampler		= VK_NULL_HANDLE;

	VkWriteDescriptorSet	writeParams = makeVkStruct();
	writeParams.dstSet				= *ds;
	writeParams.dstBinding			= b.binding;
	writeParams.dstArrayElement		= 0u;
	writeParams.descriptorCount		= 1u;
	writeParams.descriptorType		= b.descriptorType;
	writeParams.pImageInfo			= &imageInfo;

	if (VK_DESCRIPTOR_TYPE_MUTABLE_EXT == b.descriptorType)
	{
		b.view = view;
		ASSERTMSG(mutableVariant, "For VK_DESCRIPTOR_TYPE_MUTABLE_EXT mutableVariant must not be empty");
		b.mutableIndex = index_of(b.mutableTypes, *mutableVariant);
		writeParams.descriptorType = *mutableVariant;
	}

	vkUpdateDescriptorSets(*device,
						   1u,				// descriptorWriteCount
						   &writeParams,	// pDescriptorWrites
						   0u,				// copyCount,
						   nullptr			// copyParams
						   );
}
void DescriptorSetBindingManager::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZSampler sampler)
{
	add_ref<ExtBinding> b = verifyGetExtBinding(binding);
	ASSERTMSG(VK_DESCRIPTOR_TYPE_MUTABLE_EXT != b.binding
		? VK_DESCRIPTOR_TYPE_SAMPLER == b.descriptorType
		: descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_SAMPLER, b.mutableTypes), "???");
	ASSERTMSG(sampler.has_handle(), "Sampler must have a handle");

	VkDescriptorImageInfo	imageInfo{};
	imageInfo.imageLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sampler		= b.isNull ? VK_NULL_HANDLE : *sampler;
	imageInfo.imageView		= VK_NULL_HANDLE;

	VkWriteDescriptorSet	writeParams = makeVkStruct();
	writeParams.dstSet				= *ds;
	writeParams.dstBinding			= b.binding;
	writeParams.dstArrayElement		= 0u;
	writeParams.descriptorCount		= 1u;
	writeParams.descriptorType		= b.descriptorType;
	writeParams.pImageInfo			= &imageInfo;

	if (VK_DESCRIPTOR_TYPE_MUTABLE_EXT == b.descriptorType)
	{
		b.sampler = sampler;
	}

	vkUpdateDescriptorSets(*device,
							1u,				// descriptorWriteCount
							&writeParams,	// pDescriptorWrites
							0u,				// copyCount,
							nullptr			// copyParams
							);
}
void DescriptorSetBindingManager::updateDescriptorSet (
	ZDescriptorSet ds, uint32_t binding, ZImageView view,
	ZSampler sampler, VkImageLayout imageLayout)
{
	add_ref<ExtBinding> b = verifyGetExtBinding(binding);
	ASSERTMSG(VK_DESCRIPTOR_TYPE_MUTABLE_EXT != b.binding
		? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER == b.descriptorType
		: descriptorTypeOnVector(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, b.mutableTypes), "???");
	ASSERTMSG(view.has_handle() && sampler.has_handle(), "View and Sampler must have a handle");

	VkDescriptorImageInfo	imageInfo{};
	imageInfo.imageLayout	= (VK_IMAGE_LAYOUT_UNDEFINED == imageLayout)
								? b.imageLayout
								: (VK_IMAGE_LAYOUT_MAX_ENUM == imageLayout)
									? imageGetLayout(imageViewGetImage(view))
									: imageLayout;
	imageInfo.imageView = b.isNull ? VK_NULL_HANDLE : *view;
	imageInfo.sampler	= b.isNull ? VK_NULL_HANDLE : *sampler;

	VkWriteDescriptorSet	writeParams = makeVkStruct();
	writeParams.dstSet				= *ds;
	writeParams.dstBinding			= b.binding;
	writeParams.dstArrayElement		= 0u;
	writeParams.descriptorCount		= 1u;
	writeParams.descriptorType		= b.descriptorType;
	writeParams.pImageInfo			= &imageInfo;

	if (VK_DESCRIPTOR_TYPE_MUTABLE_EXT == b.descriptorType)
	{
		b.sampler = sampler;
		b.view = view;
	}

	vkUpdateDescriptorSets(*device,
							1u,				// descriptorWriteCount
							&writeParams,	// pDescriptorWrites
							0u,				// copyCount,
							nullptr			// copyParams
							);
}
bool DescriptorSetBindingManager::isDescryptorTypeSupported(VkDescriptorType type) const
{
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_SAMPLER:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
	case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
		return true;
	default:
		return false;
	}
}
void DescriptorSetBindingManager::updateDescriptorSet (ZDescriptorSet descriptorSet)
{
	for (add_cref<ExtBinding> b : m_extbindings)
	{
		if (b.descriptorType == VK_DESCRIPTOR_TYPE_MUTABLE_EXT) continue;

		switch (b.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			updateDescriptorSet(descriptorSet, b.binding, b.view, b.imageLayout);
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			updateDescriptorSet(descriptorSet, b.binding, b.buffer);
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			updateDescriptorSet(descriptorSet, b.binding, b.sampler);
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			updateDescriptorSet(descriptorSet, b.binding, b.view, b.sampler, b.imageLayout);
			break;
		default:
			ASSERTMSG(isDescryptorTypeSupported(b.descriptorType),
				"Unsupported descryptor type: ",
				vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
		}
	}
}
void assertPushConstantSizeMax (ZDevice dev, const uint32_t size)
{
	const uint32_t maxPushConstantsSize = deviceGetPhysicalLimits(dev).maxPushConstantsSize;
	ASSERTMSG(size < maxPushConstantsSize, "Push constant size exceeds device limits");
}
ZPipelineLayout DescriptorSetBindingManager::createPipelineLayout ()
{
	return createPipelineLayout_(ZPushConstants(), {});
}
ZPipelineLayout	DescriptorSetBindingManager::createPipelineLayout (add_cref<ZPushConstants> pushConstants)
{
	return createPipelineLayout_(pushConstants, {});
}
ZPipelineLayout DescriptorSetBindingManager::createPipelineLayout (
	std::initializer_list<ZDescriptorSetLayout>	dsLayouts,
	add_cref<ZPushConstants>					pushConstants)
{
	return createPipelineLayout_(pushConstants, dsLayouts);
}
ZPipelineLayout DescriptorSetBindingManager::createPipelineLayout_ (add_cref<ZPushConstants> pushConstants,
													  std::initializer_list<ZDescriptorSetLayout> dsLayouts)
{
	assertPushConstantSizeMax(device, pushConstants.size());

	const VkPipelineLayoutCreateFlags flags		(0);
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();
	ZPipelineLayout				pipelineLayout	(VK_NULL_HANDLE, device, callbacks,
												 flags,
												 std::vector<ZDescriptorSetLayout>(dsLayouts.size()),
												 pushConstants.ranges(),
												 pushConstants.types(), false);

	add_ref<std::vector<ZDescriptorSetLayout>>	zLayouts	= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	std::vector<VkDescriptorSetLayout>			vkLayouts	(dsLayouts.size());
	bool							enableDescriptorBuffer	= false;

	for (auto i = dsLayouts.begin(); i != dsLayouts.end(); ++i)
	{
		const auto j = make_unsigned(std::distance(dsLayouts.begin(), i));
		const bool descriptorBufferEnabled =
			(i->getParam<VkDescriptorSetLayoutCreateFlags>() & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
		if (enableDescriptorBuffer)
		{
			ASSERTMSG(descriptorBufferEnabled, "All descriptor set layouts must have VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT flag");
		}
		else if (descriptorBufferEnabled)
		{
			enableDescriptorBuffer = true;
		}
		zLayouts[j] = *i;
		vkLayouts[j] = **i;
	}

	add_cref<std::vector<VkPushConstantRange>> ranges =
		pipelineLayout.getParamRef<std::vector<VkPushConstantRange>>();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = makeVkStruct();
	pipelineLayoutInfo.flags					= flags;
	pipelineLayoutInfo.setLayoutCount			= data_count(vkLayouts);
	pipelineLayoutInfo.pSetLayouts				= data_or_null(vkLayouts);
	pipelineLayoutInfo.pushConstantRangeCount	= data_count(ranges);
	pipelineLayoutInfo.pPushConstantRanges		= data_or_null(ranges);

	VKASSERTMSG(vkCreatePipelineLayout(*device, &pipelineLayoutInfo, callbacks, pipelineLayout.setter()),
										"failed to create pipeline layout!");

	pipelineLayout.getParamRef<bool>() = enableDescriptorBuffer;

	return pipelineLayout;
}
ZDescriptorSet DescriptorSetBindingManager::getDescriptorSet (ZDescriptorSetLayout layout)
{
	ZDescriptorSet ds = layout.getParam<ZDescriptorSet>();
	ASSERTION(ds.has_handle());
	return ds;
}
DescriptorSetBindingManager::ExtBinding& DescriptorSetBindingManager::verifyGetExtBinding (uint32_t binding)
{
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b) { return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Binding ", binding, " not found");
	return *ptr;
}
const DescriptorSetBindingManager::ExtBinding& DescriptorSetBindingManager::verifyGetExtBinding (uint32_t binding) const
{
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Binding ", binding, " not found");
	return *ptr;
}
const DescriptorSetBindingManager::ExtBinding& DescriptorSetBindingManager::verifyGetExtBinding (std::type_index index, uint32_t binding) const
{
	const collection_element_t<decltype(m_extbindings)>& b = verifyGetExtBinding(binding);
	ASSERTMSG(b.index == index, "Mismatch type binding (", binding, ")");
	return b;
}
uint32_t DescriptorSetBindingManager::getBindingElementCount (uint32_t binding) const // TODO
{
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	if (descriptorTypeOnList(b.descriptorType, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }))
	{
        std::size_t elementSize = b.buffer.getParam<type_index_with_default>().size;
        if (0u == elementSize)
        {
            const VkFormat format = b.buffer.getParam<VkFormat>();
            ASSERTMSG(VK_FORMAT_UNDEFINED != format, "Unknown buffer format");
            elementSize = formatGetInfo(format).pixelByteSize;
        }
        const VkDeviceSize size = b.buffer.getParam<VkDeviceSize>();
        return uint32_t(size / elementSize);
	}
	ASSERTFALSE("Unsupported descryptor type: ",
		vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
	return 0u;
}

VkDeviceSize DescriptorSetBindingManager::writeResource_ (
	ZImageView view, std::type_index dataIndex, std::type_index resIndex,
	add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const
{
	UNREF(dataIndex);
	auto index = std::type_index(typeid(typename add_extent<ZImageView>::type));
	ASSERTMSG(index == resIndex, "Given resource is not an image view");

	ASSERTMSG(cmdPool,	"Command pool must have a handle, "
						"try DescriptorSetBindingManager(device, cmdPool) "
						"or DescriptorSetBindingManager(device, queue)");

	ZImage image = imageViewGetImage(view);
	ZBuffer buffer = createBuffer(image);
	const VkBufferCopy writeInfo{ 0u, 0u, std::min(bytes, bufferGetSize(buffer)) };
	bufferWriteData(buffer, data, writeInfo);

	OneShotCommandBuffer cmd(cmdPool);
	bufferCopyToImage(cmd, buffer, image,
		VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		finalLayout);

	return writeInfo.size;
}

VkDeviceSize DescriptorSetBindingManager::writeResource_ (
	ZBuffer buffer, std::type_index dataIndex, std::type_index resIndex,
	add_cptr<uint8_t> data, VkDeviceSize bytes) const
{
	UNREF(dataIndex);
	auto index = std::type_index(typeid(typename add_extent<ZBuffer>::type));
	ASSERTMSG(index == resIndex, "Given resource is not a buffer");

	const VkBufferCopy writeInfo{ 0u, 0u, std::min(bytes, bufferGetSize(buffer)) };
	bufferWriteData(buffer, data, writeInfo);

	return writeInfo.size;
}

VkDeviceSize DescriptorSetBindingManager::fillBinding_ (
	std::type_index index, uint32_t binding,
	add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const
{
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	std::vector<uint8_t> v;
	VkDeviceSize vSize = 0u;

	if (descriptorTypeOnList(b.descriptorType,
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }))
	{
		vSize = bufferGetSize(b.buffer);
	}
	else if (descriptorTypeOnList(b.descriptorType,
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }))
	{
		vSize = imageGetMipLevelsOffsetAndSize(imageViewGetImage(b.view)).second;
	}
	else
	{
		ASSERTFALSE("Unsupported descriptor type: ",
			vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
	}

	v.resize(vSize);
	const uint32_t count = uint32_t(vSize / bytes);
	for (uint32_t chunk = 0; chunk < count; ++chunk)
		std::memcpy(&v.data()[chunk * bytes], data, size_t(bytes));

	return writeBinding_(index, binding, v.data(), vSize, finalLayout);
}

VkDeviceSize DescriptorSetBindingManager::writeBinding_ (
	std::type_index index, uint32_t binding,
	add_cptr<uint8_t> data, VkDeviceSize bytes, VkImageLayout finalLayout) const
{
	VkDeviceSize saved = 0u;
	if (nullptr == data || 0u == bytes)
		return saved;

	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	switch (b.descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		saved = writeResource_(b.buffer, index, b.index, data, bytes);
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		saved = writeResource_(b.view, index, b.index, data, bytes, finalLayout);
		break;
	default:
		ASSERTFALSE("Unsupported descriptor type: ",
			vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
	}

	return saved;
}

VkDeviceSize DescriptorSetBindingManager::readResource_ (
	ZImageView view, std::type_index dataIndex, std::type_index resIndex,
	add_ptr<uint8_t> data, uint32_t elementSize, bool isVector,
	VkImageLayout finalLayout, add_cptr<VectorWrapper> pww) const
{
	auto index = std::type_index(typeid(typename add_extent<ZImageView>::type));
	ASSERTMSG(index == resIndex, "Given resource is not an image view");
	ASSERTMSG(cmdPool,	"Command pool must have a handle, "
						"try DescriptorSetBindingManager(device, cmdPool) "
						"or DescriptorSetBindingManager(device, queue)");
	ZImage image = imageViewGetImage(view);
	ZBuffer buffer = createBuffer(image);
	{
		OneShotCommandBuffer cmd(cmdPool);
		imageCopyToBuffer(cmd, image, buffer,
			VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE, VK_ACCESS_NONE,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			finalLayout);
	}
	return readResourceUnchecked_(buffer, dataIndex, data, elementSize, isVector, pww);
}

VkDeviceSize DescriptorSetBindingManager::readResourceUnchecked_ (
	ZBuffer buffer, std::type_index dataIndex, add_ptr<uint8_t> data,
	uint32_t elementSize, bool isVector, add_cptr<VectorWrapper> pww) const
{
	UNREF(dataIndex);
	const VkDeviceSize bufferSize = std::max(bufferGetSize(buffer), VkDeviceSize(elementSize));
	const uint32_t count = isVector ? uint32_t(bufferSize / elementSize) : 1u;
	if (isVector) pww->resize(count);
	add_ptr<uint8_t> usedData = isVector ? pww->data() : data;
	const VkBufferCopy readInfo{ 0u, 0u, (count * elementSize) };
	bufferReadData(buffer, usedData, readInfo);
	return readInfo.size;
}

VkDeviceSize DescriptorSetBindingManager::readResource_ (
	ZBuffer buffer, std::type_index dataIndex, std::type_index resIndex,
	add_ptr<uint8_t> data, uint32_t elementSize, bool isVector, add_cptr<VectorWrapper> pww) const
{
	auto index = std::type_index(typeid(typename add_extent<ZBuffer>::type));
	ASSERTMSG(index == resIndex, "Given resource is not a buffer");
	return readResourceUnchecked_(buffer, dataIndex, data, elementSize, isVector, pww);
}

VkDeviceSize DescriptorSetBindingManager::readBinding_ (
	std::type_index index, uint32_t binding, add_ptr<uint8_t> data,
	uint32_t elementSize, bool isVector, VkImageLayout finalLayout,
	add_cptr<VectorWrapper> pww) const
{
	VkDeviceSize readed = 0u;
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	switch (b.descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		readed = readResource_(b.buffer, index, b.index, data, elementSize, isVector, pww);
		break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		readed = readResource_(b.view, index, b.index, data, elementSize, isVector, finalLayout, pww);
		break;
	default:
		ASSERTFALSE("Unsupported descriptor type: ",
			vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
	}
	return readed;
}

size_t getDescriptorBindingSize (add_cref<VkPhysicalDeviceDescriptorBufferPropertiesEXT> p)
{
	return std::max({ p.samplerDescriptorSize
					, p.combinedImageSamplerDescriptorSize
					, p.sampledImageDescriptorSize
					, p.storageImageDescriptorSize
					, p.uniformTexelBufferDescriptorSize
					, p.robustUniformTexelBufferDescriptorSize
					, p.storageTexelBufferDescriptorSize
					, p.robustStorageTexelBufferDescriptorSize
					, p.uniformBufferDescriptorSize
					, p.robustUniformBufferDescriptorSize
					, p.storageBufferDescriptorSize
					, p.robustStorageBufferDescriptorSize
					, p.inputAttachmentDescriptorSize
					, p.accelerationStructureDescriptorSize
					});
}

bool DescriptorSetBindingManager::containsSamplers () const
{
	for (add_cref<VkDescriptorSetLayoutBindingAndType> b : m_extbindings)
	{
		if (b.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
			b.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		{
			return true;
		}
	}
	return false;
}

ZBuffer DescriptorSetBindingManager::createDescriptorBuffer (ZDescriptorSetLayout dsLayout)
{
	assertDoubledBindings();

	const uint32_t myID = getIdentifier();
	const uint32_t dsLayoutID = dsLayout.getParam<ZDistType<LayoutIdentifier, uint32_t>>();
	ASSERTMSG(myID == dsLayoutID,
		"Cannot create descriptor buffer for layout of DescriptorSetBindingManager ", dsLayoutID, " from DescriptorSetBindingManager ", myID);

	add_cref<ZDeviceInterface> di = device.getInterface();
	ASSERTMSG(di.vkGetDescriptorEXT, "vkGetDescriptorEXT() must not be null");

	VkPhysicalDeviceDescriptorBufferPropertiesEXT dbp = makeVkStruct();
	deviceGetPhysicalProperties2(device.getParam<ZPhysicalDevice>(), &dbp);

	VkDeviceSize layoutSize = 0u;
	di.vkGetDescriptorSetLayoutSizeEXT(*device, *dsLayout, &layoutSize);
	ASSERTMSG(layoutSize, "vkGetDescriptorSetLayoutSizeEXT - layout size must not be zero");
	layoutSize = ROUNDUP(layoutSize, dbp.descriptorBufferOffsetAlignment);
	//const uint32_t bindingSize = uint32_t(layoutSize);
	//layoutSize *= m_extbindings.size();

	ZBufferUsageFlags usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
							containsSamplers()
								? VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
								: VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
	ZBuffer descBuffer = createBuffer(device, layoutSize, usage, ZMemoryPropertyHostFlags);

	uint8_t* data = nullptr;
	ZDeviceMemory memory = bufferGetMemory(descBuffer, 0);
	VKASSERT(vkMapMemory(*device, *memory, 0, layoutSize, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&data)));
	std::fill_n(data, layoutSize, '\0');

	for (add_cref<VkDescriptorSetLayoutBindingAndType> b : m_extbindings)
	{
		ASSERTMSG(b.descriptorType != VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
			"In the descriptor-buffer model, none of the binding can be MUTABLE_EXT");

		VkDescriptorAddressInfoEXT	addressInfo			= makeVkStruct();
		VkDescriptorGetInfoEXT		getInfo				= makeVkStruct();
		VkDescriptorImageInfo		imageInfo			{};
		VkDeviceSize				descriptorOffset	= 0;
		size_t						writeDescriptorSize	= 0;

		getInfo.type = b.descriptorType;
		di.vkGetDescriptorSetLayoutBindingOffsetEXT(*device, *dsLayout, b.binding, &descriptorOffset);

		if (descriptorTypeOnList(b.descriptorType, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}))
		{
			if (false == b.isNull)
			{
				ZBuffer buffer = b.buffer;
				addressInfo.address = bufferGetAddress(buffer, b.binding, b.descriptorType);
				addressInfo.range = bufferGetSize(buffer);
				addressInfo.format = buffer.getParam<VkFormat>();
			}
			if (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				getInfo.data.pUniformBuffer = &addressInfo;
				writeDescriptorSize = dbp.uniformBufferDescriptorSize;
			}
			else
			{
				getInfo.data.pStorageBuffer = &addressInfo;
				writeDescriptorSize = dbp.storageBufferDescriptorSize;
			}
		}
		else
		{
			switch (b.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			{
				if (false == b.isNull)
				{
					ZImageView	view = b.view;
					imageInfo.imageView = *view;
					imageInfo.imageLayout = b.imageLayout;
				}
				if (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				{
					getInfo.data.pStorageImage = &imageInfo;
					writeDescriptorSize = dbp.storageImageDescriptorSize;
				}
				else
				{
					getInfo.data.pSampledImage = &imageInfo;
					writeDescriptorSize = dbp.sampledImageDescriptorSize;
				}
			}
			break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			{
				getInfo.data.pSampler = b.isNull ? nullptr : b.sampler.ptr();
				writeDescriptorSize = dbp.samplerDescriptorSize;
			}
			break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				if (false == b.isNull)
				{
					ZImageView	view = b.view;
					ZSampler	samp = b.sampler;
					if (dbp.combinedImageSamplerDescriptorSingleArray)
					{
						// TODO
					}
					imageInfo.sampler = *samp;
					imageInfo.imageView = *view;
					imageInfo.imageLayout = b.imageLayout;
				}
				getInfo.data.pCombinedImageSampler = &imageInfo;
				writeDescriptorSize = dbp.combinedImageSamplerDescriptorSize;
			}
			break;
			default:
				ASSERTFALSE("Unsupported descryptor type: ",
					vk::to_string(static_cast<vk::DescriptorType>(b.descriptorType)));
			}
		}

		ASSERTMSG((descriptorOffset + writeDescriptorSize) <= layoutSize, "");
		di.vkGetDescriptorEXT(*device, &getInfo, writeDescriptorSize, (data + descriptorOffset));
	}

	vkUnmapMemory(*device, *memory);

	return descBuffer;
}

} // namespace vtf

