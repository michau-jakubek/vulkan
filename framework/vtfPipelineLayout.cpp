#include "vtfPipelineLayout.hpp"
#include "vtfZUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfCUtils.hpp"

static std::pair<VkDescriptorType, VkBufferUsageFlagBits>
const DescriptorTypeToBufferUsage[]
{
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT	},
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT	},
};
static bool isImageDescriptorType (const VkDescriptorType type)
{
	return (	type == VK_DESCRIPTOR_TYPE_SAMPLER
			 || type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
			 || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			 || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}
static bool isBufferDescriptorType (const VkDescriptorType type)
{
	return (	type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			 || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
			 || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			 || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
			 || type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
			 || type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
}

#define UNIQUE_IBINDING std::numeric_limits<int>::min()

namespace vtf
{

PipelineLayout::PipelineLayout (ZDevice device)
	: m_device				(device)
	, m_extbindings			()
	, m_views				()
	, m_samplers			()
	, m_viewsAndSamplers	()
	//, m_texelBuffers		()
	, m_buffers				()
{
}
uint32_t PipelineLayout::joinBindings_ (std::type_index index, VkDeviceSize size, bool isVector,
										VkDescriptorType type, VkShaderStageFlags stages, uint32_t count)
{
	ASSERTMSG(0 != count, "Descriptor count must not be zero!");
	ASSERTMSG(isBufferDescriptorType(type), "Wrong descriptor type");
	const uint32_t binding = static_cast<uint32_t>(m_extbindings.size());
	VkDescriptorSetLayoutBindingAndType b;
	for (uint32_t i = 0; i < count; ++i)
	{
		b.binding				= static_cast<uint32_t>(m_extbindings.size());
		b.descriptorCount		= 1;
		b.descriptorType		= type;
		b.pImmutableSamplers	= nullptr;
		b.stageFlags			= stages;
		b.index					= index;
		b.size					= size;
		b.offset				= 0;
		b.elementCount			= 1;
		b.isVector				= isVector;
		b.shared				= true;
		m_extbindings.emplace_back(b);
	}
	return binding;
}
uint32_t PipelineLayout::addBinding_ (std::type_index index, VkDeviceSize size, bool isVector,
									  VkDescriptorType type, VkShaderStageFlags stages, uint32_t elementCount)
{
	VkDescriptorSetLayoutBindingAndType b;
	const uint32_t binding = static_cast<uint32_t>(m_extbindings.size());
	b.binding				= binding;
	b.descriptorCount		= 1;
	b.descriptorType		= type;
	b.pImmutableSamplers	= nullptr;
	b.stageFlags			= stages;
	b.index					= index;
	b.size					= size;
	b.offset				= 0;
	b.elementCount			= elementCount;
	b.isVector				= isVector;
	b.shared				= false;
	m_extbindings.emplace_back(b);
	return binding;
}
uint32_t PipelineLayout::addBinding (ZImageView view, ZSampler sampler,
									 VkDescriptorType type, VkShaderStageFlags stages)
{
	VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	if (VK_DESCRIPTOR_TYPE_MAX_ENUM == type)
	{
		if (sampler.has_handle())
		{
			if (view.has_handle())
				descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			else descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		}
		else if (view.has_handle())
			descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		else
		{
			ASSERTMSG(false, "At least of view and sampler must have a handle");
		}
	}
	else
	{
		switch (type)
		{
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			ASSERTION(view.has_handle() && sampler.has_handle());
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			ASSERTION(sampler.has_handle());
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			ASSERTION(view.has_handle());
			break;
		default:
			ASSERTION(0);
			break;
		}
		descriptorType = type;
	}

	const uint32_t binding = addBinding(descriptorType, stages);

	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		m_viewsAndSamplers[m_extbindings[binding].offset].first = view;
		m_viewsAndSamplers[m_extbindings[binding].offset].second = sampler;
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		m_samplers[m_extbindings[binding].offset] = sampler;
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		m_views[m_extbindings[binding].offset] = view;
		break;
	default:
		ASSERTION(0);
		break;
	}

	return binding;
}
uint32_t PipelineLayout::addBinding (VkDescriptorType type, VkShaderStageFlags stages)
{
	VkDescriptorSetLayoutBindingAndType b;
	const uint32_t binding = static_cast<uint32_t>(m_extbindings.size());
	b.binding				= binding;
	b.descriptorCount		= 1;
	b.descriptorType		= type;
	b.pImmutableSamplers	= nullptr;
	b.stageFlags			= stages;
	b.size					= 0;
	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		b.index				= std::type_index(typeid(typename add_extent<ZImageView>::type));
		b.offset			= m_views.size();
		m_views.push_back(ZImageView());
		break;
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		b.index				= std::type_index(typeid(typename add_extent<ZSampler>::type));
		b.offset			= m_samplers.size();
		m_samplers.push_back(ZSampler());
		break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		b.index				= std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type));
		b.offset			= m_viewsAndSamplers.size();
		m_viewsAndSamplers.push_back({ ZImageView(), ZSampler() });
		break;
	default:	ASSERTION(false);
	}
	b.elementCount			= 0;
	b.isVector				= false;
	b.shared				= false;
	m_extbindings.emplace_back(b);

	return binding;
}
ZDescriptorSetLayout PipelineLayout::getDescriptorSetLayout (ZPipelineLayout layout, uint32_t index)
{
	add_ref<std::vector<ZDescriptorSetLayout>> dsLayouts
			= layout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	return dsLayouts[index];
}
ZDescriptorSetLayout PipelineLayout::createDescriptorSetLayout (bool performUpdateDescriptorSets)
{
	VkAllocationCallbacksPtr					callbacks			= m_device.getParam<VkAllocationCallbacksPtr>();
	ZDescriptorPool								descriptorPool		(VK_NULL_HANDLE, m_device, callbacks);
	ZDescriptorSetLayout						descriptorSetLayout	(VK_NULL_HANDLE, m_device, callbacks, ZDescriptorSet());
	std::vector<VkDescriptorSetLayoutBinding>	bindings			(m_extbindings.size());

	// build descriptor pool
	{
		std::vector<VkDescriptorPoolSize>		poolSizes;
		std::map<VkDescriptorType, uint32_t>	typesNsizes;

		uint32_t bid = 0;
		for (add_cref<VkDescriptorSetLayoutBinding> b : m_extbindings)
		{
			typesNsizes[b.descriptorType] += b.descriptorCount;
			bindings[bid++] = b;
		}

		for (const auto& typeNsize : typesNsizes)
			poolSizes.push_back({typeNsize.first, typeNsize.second});

		VkDescriptorPoolCreateInfo		poolCreateInfo{};
		poolCreateInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.pNext			= nullptr;
		poolCreateInfo.flags			= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolCreateInfo.maxSets			= 1;
		poolCreateInfo.poolSizeCount	= data_count(poolSizes);
		poolCreateInfo.pPoolSizes		= data_or_null(poolSizes);
		VKASSERT(vkCreateDescriptorPool(*m_device, &poolCreateInfo, callbacks,
										descriptorPool.setter()), "Unable to create descriptor pool");
	}

	VkDescriptorSetLayoutCreateInfo		setLayoutCreateInfo{};
	setLayoutCreateInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.pNext			= nullptr;
	setLayoutCreateInfo.flags			= 0;
	setLayoutCreateInfo.bindingCount	= data_count(bindings);
	setLayoutCreateInfo.pBindings		= data_or_null(bindings);
	VKASSERT(vkCreateDescriptorSetLayout(*m_device, &setLayoutCreateInfo, callbacks,
										 descriptorSetLayout.setter()), "Failed to create descriptor set layout");

	updateBuffersOffsets();
	recreateUpdateBuffers(m_buffers);

	ZDescriptorSet					descriptorSet(VK_NULL_HANDLE, m_device, descriptorPool);
	VkDescriptorSetAllocateInfo		allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext					= nullptr;
	allocInfo.descriptorPool		= *descriptorPool;
	allocInfo.descriptorSetCount	= 1;
	allocInfo.pSetLayouts			= descriptorSetLayout.ptr();
	VKASSERT(vkAllocateDescriptorSets(*m_device, &allocInfo,
									  descriptorSet.setter()), "Failed to allocate descriptor set");

	if (performUpdateDescriptorSets)
	{
		updateDescriptorSet_(descriptorSet, m_buffers);
	}

	descriptorSetLayout.setParam<ZDescriptorSet>(descriptorSet);

	return descriptorSetLayout;
}
VkDeviceSize PipelineLayout::getDescriptorAlignment (const VkDescriptorType type) const
{
	VkDeviceSize alignment = 16u;
	switch (type)
	{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			alignment = m_device.getParam<ZPhysicalDevice>()
					.getParamRef<VkPhysicalDeviceProperties>().limits.minUniformBufferOffsetAlignment;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			alignment = m_device.getParam<ZPhysicalDevice>()
					.getParamRef<VkPhysicalDeviceProperties>().limits.minStorageBufferOffsetAlignment;
			break;

		default:
			alignment = 16u;
	}
	return alignment;
}
void PipelineLayout::updateBuffersOffsets ()
{
	std::map<VkDescriptorType, VkDeviceSize>	offsets;

	for (add_ref<collection_element_t<decltype(m_extbindings)>> b : m_extbindings)
	{
		if (isBufferDescriptorType(b.descriptorType) && b.shared)
		{
			b.offset = offsets[b.descriptorType];
			offsets[b.descriptorType] += ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
		}
	}
}
void PipelineLayout::recreateUpdateBuffers (std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers)
{
	std::map<std::pair<VkDescriptorType, int>, VkDeviceSize>	sizes;

	for (add_ref<collection_element_t<decltype(m_extbindings)>> b : m_extbindings)
	{
		if (isBufferDescriptorType(b.descriptorType))
		{
			if (b.shared)
			{
				sizes[{b.descriptorType, UNIQUE_IBINDING}] +=
						ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
			}
			else
			{
				sizes[{b.descriptorType, static_cast<int>(b.binding)}] =
						ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
			}
		}
	}

	buffers.clear();

	for (const auto& size : sizes)
	{
		auto usagePtr = std::find_if(std::begin(DescriptorTypeToBufferUsage), std::end(DescriptorTypeToBufferUsage),
									 [&size](const auto& x) { return x.first == size.first.first; });
		ASSERTION(std::end(DescriptorTypeToBufferUsage) != usagePtr);

		const ZBufferUsageFlags		usage	(usagePtr->second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const ZMemoryPropertyFlags	flags	(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		ZBuffer buffer = createBuffer(m_device, size.second, usage, flags);

		buffers.emplace(size.first, buffer);
	}
}
void PipelineLayout::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZImageView view)
{
	ASSERTMSG(view.has_handle(), "ImageView must have a handle");
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Cannot find desired binding");
	if (ptr->index == std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type)))
		m_viewsAndSamplers[ptr->offset].first = view;
	else if (ptr->index == std::type_index(typeid(typename add_extent<ZImageView>::type)))
		m_views[ptr->offset] = view;
	else
	{
		ASSERTMSG(false, "Mismatch type binding");
	}
	if (ds.has_handle())
	{
		updateDescriptorSet_(ds, m_buffers);
	}
}
void PipelineLayout::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZSampler sampler)
{
	ASSERTMSG(sampler.has_handle(), "Sampler must have a handle");
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Cannot find desired binding");
	if (ptr->index == std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type)))
		m_viewsAndSamplers[ptr->offset].second = sampler;
	else if (ptr->index == std::type_index(typeid(typename add_extent<ZSampler>::type)))
		m_samplers[ptr->offset] = sampler;
	else
	{
		ASSERTMSG(false, "Mismatch type binding");
	}
	if (ds.has_handle())
	{
		updateDescriptorSet_(ds, m_buffers);
	}
}
void PipelineLayout::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZImageView view, ZSampler sampler)
{
	const bool viewHasHandle = view.has_handle();
	const bool samplerHasHandle = sampler.has_handle();
	ASSERTMSG(viewHasHandle && samplerHasHandle, "ImageView and Sampler must have a handle");
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Cannot find desired binding");
	if (ptr->index == std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type)))
	{
		m_viewsAndSamplers[ptr->offset].first	= view;
		m_viewsAndSamplers[ptr->offset].second	= sampler;
	}
	else
	{
		ASSERTMSG(false, "Mismatch type binding");
	}
	if (ds.has_handle())
	{
		updateDescriptorSet_(ds, m_buffers);
	}
}
void PipelineLayout::updateDescriptorSet_	(ZDescriptorSet	descriptorSet,
											 std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers) const
{
	for (add_cref<VkDescriptorSetLayoutBindingAndType> b : m_extbindings)
	{
		VkDescriptorImageInfo	imageInfo{};	UNREF(imageInfo);
		VkDescriptorBufferInfo	bufferInfo{};	UNREF(bufferInfo);
		VkBufferView			bufferView{};	UNREF(bufferView);

		VkWriteDescriptorSet	writeParams{};
		writeParams.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeParams.pNext				= nullptr;
		writeParams.dstSet				= *descriptorSet;
		writeParams.dstBinding			= b.binding;
		writeParams.dstArrayElement		= 0;
		writeParams.descriptorCount		= 1;
		writeParams.descriptorType		= b.descriptorType;

		writeParams.pImageInfo			= nullptr;
		writeParams.pTexelBufferView	= nullptr;
		writeParams.pBufferInfo			= nullptr;

		if (isImageDescriptorType(b.descriptorType))
		{
			switch (b.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				{
					ZImageView	view		= m_views[b.offset];
					// TODO:
					//ZImage		img			= view.getParam<ZImage>();
					imageInfo.imageView		= *view;
					imageInfo.imageLayout	= VK_IMAGE_LAYOUT_GENERAL; // ??? img.getParamRef<VkImageCreateInfo>().initialLayout;
				}
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				{
					imageInfo.sampler	= *m_samplers[b.offset];
				}
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				{
					ZImageView	view		= m_viewsAndSamplers[b.offset].first;
					ZSampler	samp		= m_viewsAndSamplers[b.offset].second;
					ZImage		img			= view.getParam<ZImage>();
					imageInfo.imageView		= *view;
					imageInfo.imageLayout	= VK_IMAGE_LAYOUT_GENERAL; // ??? img.getParamRef<VkImageCreateInfo>().initialLayout;
					imageInfo.sampler		= *samp;
				}
				break;
			default:	ASSERTION(false);
			}
			writeParams.pImageInfo	= &imageInfo;
		}
		else if (isBufferDescriptorType(b.descriptorType))
		{
			if (b.shared)
			{
				bufferInfo.buffer			= *buffers.at({b.descriptorType, UNIQUE_IBINDING});
			}
			else
			{
				bufferInfo.buffer			= *buffers.at({b.descriptorType, static_cast<int>(b.binding)});
			}
			bufferInfo.offset				= b.offset;
			bufferInfo.range				= ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
			writeParams.pBufferInfo			= &bufferInfo;
		}
		else
		{
			ASSERTION(0);
		}

		vkUpdateDescriptorSets(*m_device,
							   1,				// descriptorWriteCount
							   &writeParams,	// pDescriptorWrites
							   0,				// copyCount,
							   nullptr			// copyParams
							   );
	}
}
void assertPushConstantSizeMax (ZPhysicalDevice dev, const uint32_t size)
{
	const uint32_t maxPushConstantsSize = dev.getParamRef<VkPhysicalDeviceProperties>().limits.maxPushConstantsSize;
	ASSERTION(size < maxPushConstantsSize);
}
ZPipelineLayout PipelineLayout::createPipelineLayout ()
{
	return createPipelineLayout_({}, nullptr, type_index_with_default());
}
ZPipelineLayout PipelineLayout::createPipelineLayout (ZDescriptorSetLayout dsLayout)
{
	return createPipelineLayout_({dsLayout}, nullptr, type_index_with_default());
}
ZPipelineLayout PipelineLayout::createPipelineLayout_ (std::initializer_list<ZDescriptorSetLayout> descriptorSetLayouts,
													   const VkPushConstantRange* pPushConstantRanges, type_index_with_default typeOfPushConstant)
{
	VkPushConstantRange			emptyRange		{};
	VkAllocationCallbacksPtr	callbacks		= m_device.getParam<VkAllocationCallbacksPtr>();
	ZPipelineLayout				pipelineLayout	(VK_NULL_HANDLE, m_device, callbacks, {},
												 (pPushConstantRanges ? *pPushConstantRanges : emptyRange), typeOfPushConstant);

	add_ref<std::vector<ZDescriptorSetLayout>>	zLayouts	= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	std::vector<VkDescriptorSetLayout>			vkLayouts	(descriptorSetLayouts.size());

	for (auto i = descriptorSetLayouts.begin(); i != descriptorSetLayouts.end(); ++i)
	{
		zLayouts.emplace_back(*i);
		vkLayouts[std::distance(descriptorSetLayouts.begin(), i)] = **i;
	}

	if (pPushConstantRanges)
	{
		assertPushConstantSizeMax(m_device.getParam<ZPhysicalDevice>(),
			pPushConstantRanges[0].offset + pPushConstantRanges[0].size);
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext					= nullptr;
	pipelineLayoutInfo.flags					= 0;
	pipelineLayoutInfo.setLayoutCount			= data_count(vkLayouts);
	pipelineLayoutInfo.pSetLayouts				= data_or_null(vkLayouts);
	pipelineLayoutInfo.pushConstantRangeCount	= pPushConstantRanges ? 1 : 0;
	pipelineLayoutInfo.pPushConstantRanges		= pPushConstantRanges;

	VKASSERT(vkCreatePipelineLayout(*m_device, &pipelineLayoutInfo, callbacks, pipelineLayout.setter()),
										"failed to create pipeline layout!");

	return pipelineLayout;
}
ZDescriptorSet PipelineLayout::getDescriptorSet (ZDescriptorSetLayout layout)
{
	ZDescriptorSet ds = layout.getParam<ZDescriptorSet>();
	ASSERTION(ds.has_handle());
	return ds;
}
VarDescriptorInfo PipelineLayout::getDescriptorInfo (uint32_t binding)
{
	VarDescriptorInfo var;
	const collection_element_t<decltype(m_extbindings)>& b = verifyGetExtBinding(binding);
	if (isBufferDescriptorType(b.descriptorType))
	{
		VkDeviceSize range = ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
		auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
		var.emplace<DescriptorBufferInfo>(buffer, b.offset, range);
	}
	else if (isImageDescriptorType(b.descriptorType))
	{
		ZImageView		view;
		ZImage			image;
		ZSampler		sampler;
		switch (b.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			sampler = m_samplers[b.offset];
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			view = m_views[b.offset];
			image = view.getParam<ZImage>();
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			sampler = m_viewsAndSamplers[b.offset].second;
			view = m_viewsAndSamplers[b.offset].first;
			image = view.getParam<ZImage>();
			break;
		default:
			ASSERT_NOT_IMPLEMENTED();
			break;
		}
		var.emplace<DescriptorImageInfo>(sampler, view, image, imageGetLayout(image));
	}
	return var;
}
const PipelineLayout::ExtBinding& PipelineLayout::verifyGetExtBinding (uint32_t binding) const
{
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Mismatch type binding");
	return *ptr;
}
const PipelineLayout::ExtBinding& PipelineLayout::verifyGetExtBinding (std::type_index index, uint32_t binding) const
{
	const collection_element_t<decltype(m_extbindings)>& b = verifyGetExtBinding(binding);
	ASSERTMSG(b.index == index, "");
	return b;
}
void PipelineLayout::zeroBinding (uint32_t binding)
{
	uint8_t data[256];
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	for (uint32_t i = 0; i < ARRAY_LENGTH(data); ++i) data[i] = 0;
	const VkDeviceSize fullSize = b.size * b.elementCount;
	const VkDeviceSize chunkCount = fullSize / ARRAY_LENGTH(data);
	const VkDeviceSize orphanSize = fullSize % ARRAY_LENGTH(data);
	auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	for (VkDeviceSize i = 0; i < chunkCount; ++i)
	{
		const VkBufferCopy writeInfo
		{
			0,
			b.offset + i * ARRAY_LENGTH(data),
			ARRAY_LENGTH(data)
		};
		bufferWriteData(buffer, data, writeInfo, false);
	}
	if (orphanSize)
	{
		const VkBufferCopy writeInfo
		{
			0,
			b.offset + chunkCount * ARRAY_LENGTH(data),
			orphanSize,
		};
		bufferWriteData(buffer, data, writeInfo, false);
	}
	bufferFlush(buffer);
}
void PipelineLayout::writeBinding_ (std::type_index index, uint32_t binding, const uint8_t* data, VkDeviceSize bytes)
{
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	const VkBufferCopy writeInfo { 0, b.offset, std::min((b.size * b.elementCount), bytes) };
	auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	bufferWriteData(buffer, data, writeInfo);
}
void PipelineLayout::readBinding_ (std::type_index index, uint32_t binding, uint8_t* data, VkDeviceSize bytes) const
{
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	const VkBufferCopy readInfo { b.offset, 0, std::min((b.size * b.elementCount), bytes) };
	auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	bufferReadData(buffer, data, readInfo);
}
void PipelineLayout::getBinding_ (uint32_t binding, std::optional<ZImageView>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<ZImageView>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_views[b.offset];
}
void PipelineLayout::getBinding_ (uint32_t binding, std::optional<ZSampler>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<ZSampler>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_samplers[b.offset];
}
void PipelineLayout::getBinding_ (uint32_t binding, std::optional<std::pair<ZImageView,ZSampler>>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_viewsAndSamplers[b.offset];
}

} // namespace vtf

