#include "vtfLayoutManager.hpp"
#include "vtfZUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfCUtils.hpp"
#include "vtfStructUtils.hpp"

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

LayoutManager::LayoutManager (ZDevice device)
	: device				(device)
	, m_extbindings			()
	, m_views				()
	, m_samplers			()
	, m_viewsAndSamplers	()
	//, m_texelBuffers		()
	, m_buffers				()
{
}
uint32_t LayoutManager::joinBindings_ (std::type_index		index,
									   VkDeviceSize			size,
									   bool					isVector,
									   VkDescriptorType		type,
									   VkShaderStageFlags	stages,
									   uint32_t				count)
{
	ASSERTMSG(0 != count, "Descriptor count must not be zero!");
	ASSERTMSG(isBufferDescriptorType(type),
			  "Descriptor type must not be image, sampler nor combined image sampler");
	const uint32_t binding = data_count(m_extbindings);
	VkDescriptorSetLayoutBindingAndType b;
	for (uint32_t i = 0; i < count; ++i)
	{
		b.binding				= data_count(m_extbindings);
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
		b.imageLayout			= VK_IMAGE_LAYOUT_GENERAL;
		m_extbindings.emplace_back(b);
	}
	return binding;
}
uint32_t LayoutManager::addBinding_ (std::type_index	index,
									 VkDeviceSize		size,
									 bool				isVector,
									 VkDescriptorType	type,
									 VkShaderStageFlags	stages,
									 uint32_t			elementCount)
{
	VkDescriptorSetLayoutBindingAndType b;
	const uint32_t binding = data_count(m_extbindings);
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
	b.imageLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	m_extbindings.emplace_back(b);
	return binding;
}
uint32_t LayoutManager::addBinding (ZImageView			view,
									ZSampler			sampler,
									VkImageLayout		imageLayout,
									VkDescriptorType	type,
									VkShaderStageFlags	stages)
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
		{
			descriptorType = (view.getParam<ZImage>().getParamRef<VkImageCreateInfo>().usage & VK_IMAGE_USAGE_STORAGE_BIT)
				? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
				: VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		}
		else
		{
			ASSERTFALSE("At least of view and sampler must have a handle");
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
			ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
			break;
		}
		descriptorType = type;
	}

	const uint32_t binding = addBinding(descriptorType, imageLayout, stages);

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
		ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
		break;
	}

	return binding;
}
uint32_t LayoutManager::addBinding (VkDescriptorType	type,
									VkImageLayout		imageLayout,
									VkShaderStageFlags	stages)
{
	VkDescriptorSetLayoutBindingAndType b;
	const uint32_t binding = data_count(m_extbindings);
	b.binding				= binding;
	b.descriptorCount		= 1;
	b.descriptorType		= type;
	b.pImmutableSamplers	= nullptr;
	b.stageFlags			= stages;
	b.size					= 0;
	b.imageLayout			= imageLayout;
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
	default:	ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
	}
	b.elementCount			= 0;
	b.isVector				= false;
	b.shared				= false;
	m_extbindings.emplace_back(b);

	return binding;
}
ZDescriptorSetLayout LayoutManager::getDescriptorSetLayout (ZPipelineLayout layout, uint32_t index)
{
	add_ref<std::vector<ZDescriptorSetLayout>> dsLayouts
			= layout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	return dsLayouts[index];
}
ZDescriptorSetLayout LayoutManager::createDescriptorSetLayout (
	bool								performUpdateDescriptorSets,
	VkDescriptorSetLayoutCreateFlags	layoutCreateFlags,
	VkDescriptorPoolCreateFlags			poolCreateFlags)
{
	VkAllocationCallbacksPtr					callbacks			= device.getParam<VkAllocationCallbacksPtr>();
	ZDescriptorPool								descriptorPool		(VK_NULL_HANDLE, device, callbacks);
	ZDescriptorSetLayout						descriptorSetLayout	(VK_NULL_HANDLE, device, callbacks, ZDescriptorSet());
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

		VkDescriptorPoolCreateInfo		poolCreateInfo = makeVkStruct();
		poolCreateInfo.flags			= poolCreateFlags;
		poolCreateInfo.maxSets			= 1;
		poolCreateInfo.poolSizeCount	= data_count(poolSizes);
		poolCreateInfo.pPoolSizes		= data_or_null(poolSizes);
		VKASSERTMSG(vkCreateDescriptorPool(*device, &poolCreateInfo, callbacks,
										   descriptorPool.setter()), "Unable to create descriptor pool");
	}

	VkDescriptorSetLayoutCreateInfo		setLayoutCreateInfo = makeVkStruct();
	setLayoutCreateInfo.flags			= layoutCreateFlags;
	setLayoutCreateInfo.bindingCount	= data_count(bindings);
	setLayoutCreateInfo.pBindings		= data_or_null(bindings);
	VKASSERTMSG(vkCreateDescriptorSetLayout(*device, &setLayoutCreateInfo, callbacks,
											descriptorSetLayout.setter()), "Failed to create descriptor set layout");

	updateBuffersOffsets();
	recreateUpdateBuffers(m_buffers, performUpdateDescriptorSets);

	ZDescriptorSet					descriptorSet(VK_NULL_HANDLE, device, descriptorPool);
	VkDescriptorSetAllocateInfo		allocInfo = makeVkStruct();
	allocInfo.descriptorPool		= *descriptorPool;
	allocInfo.descriptorSetCount	= 1;
	allocInfo.pSetLayouts			= descriptorSetLayout.ptr();
	VKASSERTMSG(vkAllocateDescriptorSets(*device, &allocInfo,
										 descriptorSet.setter()), "Failed to allocate descriptor set");

	if (performUpdateDescriptorSets)
	{
		updateDescriptorSet_(descriptorSet, m_buffers);
	}

	descriptorSetLayout.setParam<ZDescriptorSet>(descriptorSet);

	return descriptorSetLayout;
}
VkDeviceSize LayoutManager::getDescriptorAlignment (const VkDescriptorType type) const
{
	VkDeviceSize alignment = 16u;
	switch (type)
	{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			alignment = deviceGetPhysicalLimits(device).minUniformBufferOffsetAlignment;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			alignment = deviceGetPhysicalLimits(device).minStorageBufferOffsetAlignment;
			break;

		default:
			alignment = 16u;
	}
	return alignment;
}
void LayoutManager::updateBuffersOffsets ()
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
void LayoutManager::recreateUpdateBuffers (std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers, bool performUpdateDescriptorSets)
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

		ZBuffer buffer;
		if (performUpdateDescriptorSets)
		{
			buffer = createBuffer(device, size.second, usage, flags);
		}

		buffers.emplace(size.first, buffer);
	}
}
void LayoutManager::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZBuffer buffer)
{
	ASSERTMSG(ds.has_handle(), "Descriptor set must have a handle");
	ASSERTMSG(buffer.has_handle(), "Buffer must have a handle");
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Cannot find desired binding");
	ZBuffer meBuffer = m_buffers.at({ptr->descriptorType, (ptr->shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	if (meBuffer.has_handle())
	{
		const VkBufferUsageFlags meUsageFlags = meBuffer.getParamRef<VkBufferCreateInfo>().usage;
		const VkBufferUsageFlags bufferUsageFlags = buffer.getParamRef<VkBufferCreateInfo>().usage;
		ASSERTMSG((meUsageFlags & bufferUsageFlags), "Buffers usage flags must match");
	}
	else
	{
		auto usagePtr = std::find_if(std::begin(DescriptorTypeToBufferUsage), std::end(DescriptorTypeToBufferUsage),
			[&](const auto& x) { return x.first == ptr->descriptorType; });
		ASSERTION(std::end(DescriptorTypeToBufferUsage) != usagePtr);
		const ZBufferUsageFlags meUsageFlags(usagePtr->second, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const VkBufferUsageFlags bufferUsageFlags = buffer.getParamRef<VkBufferCreateInfo>().usage;
		ASSERTMSG((meUsageFlags() & bufferUsageFlags), "Buffers usage flags must match");
	}
	ptr->elementCount = ptr->isVector ? static_cast<uint32_t>(bufferGetSize(buffer) / ptr->size) : 1u;
	m_buffers.at({ptr->descriptorType, (ptr->shared ? UNIQUE_IBINDING : static_cast<int>(binding))}) = buffer;
	updateDescriptorSet_(ds, m_buffers);
}
void LayoutManager::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZImageView view)
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
		ASSERTFALSE("Mismatch type binding");
	}
	updateDescriptorSet_(ds, m_buffers);
}
void LayoutManager::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZSampler sampler)
{
	ASSERTMSG(ds.has_handle(), "Descriptor set must have a handle");
	ASSERTMSG(sampler.has_handle(), "Sampler must have a handle");
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Cannot find desired binding");
	if (ptr->index == std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type)))
		m_viewsAndSamplers[ptr->offset].second = sampler;
	else if (ptr->index == std::type_index(typeid(typename add_extent<ZSampler>::type)))
		m_samplers[ptr->offset] = sampler;
	else
	{
		ASSERTFALSE("Mismatch type binding");
	}
	updateDescriptorSet_(ds, m_buffers);
}
void LayoutManager::updateDescriptorSet (ZDescriptorSet ds, uint32_t binding, ZImageView view, ZSampler sampler)
{
	const bool viewHasHandle = view.has_handle();
	const bool samplerHasHandle = sampler.has_handle();
	ASSERTMSG(ds.has_handle(), "Descriptor set must have a handle");
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
		ASSERTFALSE("Mismatch type binding");
	}
	updateDescriptorSet_(ds, m_buffers);
}
void LayoutManager::updateDescriptorSet_	(ZDescriptorSet	descriptorSet,
											 std::map<std::pair<VkDescriptorType, int>, ZBuffer>& buffers) const
{
	for (add_cref<VkDescriptorSetLayoutBindingAndType> b : m_extbindings)
	{
		VkDescriptorImageInfo	imageInfo{};	UNREF(imageInfo);
		VkDescriptorBufferInfo	bufferInfo{};	UNREF(bufferInfo);
		VkBufferView			bufferView{};	UNREF(bufferView);

		VkWriteDescriptorSet	writeParams = makeVkStruct();
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
					imageInfo.imageView		= *view;
					imageInfo.imageLayout	= b.imageLayout;
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
					imageInfo.imageView		= *view;
					imageInfo.imageLayout	= b.imageLayout;
					imageInfo.sampler		= *samp;
				}
				break;
			default:	ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
			}
			writeParams.pImageInfo	= &imageInfo;
		}
		else if (isBufferDescriptorType(b.descriptorType))
		{
			VkDeviceSize range = 0u;
			if (b.shared)
			{
				bufferInfo.buffer			= *buffers.at({b.descriptorType, UNIQUE_IBINDING});
				range = ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
			}
			else
			{
				bufferInfo.buffer			= *buffers.at({b.descriptorType, static_cast<int>(b.binding)});
				range = b.size * b.elementCount;
			}
			bufferInfo.offset				= b.offset;
			bufferInfo.range				= ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
			bufferInfo.range				= range;
			writeParams.pBufferInfo			= &bufferInfo;
		}
		else
		{
			ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
		}

		vkUpdateDescriptorSets(*device,
							   1,				// descriptorWriteCount
							   &writeParams,	// pDescriptorWrites
							   0,				// copyCount,
							   nullptr			// copyParams
							   );
	}
}
void assertPushConstantSizeMax (ZDevice dev, const uint32_t size)
{
	const uint32_t maxPushConstantsSize = deviceGetPhysicalLimits(dev).maxPushConstantsSize;
	ASSERTMSG(size < maxPushConstantsSize, "Push constant size exceeds device limits");
}
ZPipelineLayout LayoutManager::createPipelineLayout ()
{
	return createPipelineLayout_(ZPushConstants(), {});
}
ZPipelineLayout	LayoutManager::createPipelineLayout (add_cref<ZPushConstants> pushConstants)
{
	return createPipelineLayout_(pushConstants, {});
}
ZPipelineLayout LayoutManager::createPipelineLayout (ZDescriptorSetLayout dsLayout,
													 add_cref<ZPushConstants> pushConstants)
{
	ASSERTMSG(dsLayout.has_handle(), "DescriptorSetLayout must have a valid handle");
	return createPipelineLayout_(pushConstants, { dsLayout });
}
ZPipelineLayout LayoutManager::createPipelineLayout_ (add_cref<ZPushConstants> pushConstants,
													  std::initializer_list<ZDescriptorSetLayout> dsLayouts)
{
	assertPushConstantSizeMax(device, pushConstants.size());

	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();
	ZPipelineLayout				pipelineLayout	(VK_NULL_HANDLE, device, callbacks,
												 std::vector<ZDescriptorSetLayout>(dsLayouts.size()),
												 pushConstants.ranges(),
												 pushConstants.types());

	add_ref<std::vector<ZDescriptorSetLayout>>	zLayouts	= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();
	std::vector<VkDescriptorSetLayout>			vkLayouts	(dsLayouts.size());

	for (auto i = dsLayouts.begin(); i != dsLayouts.end(); ++i)
	{
		const auto j = make_unsigned(std::distance(dsLayouts.begin(), i));
		zLayouts[j] = *i;
		vkLayouts[j] = **i;
	}

	add_cref<std::vector<VkPushConstantRange>> ranges =
		pipelineLayout.getParamRef<std::vector<VkPushConstantRange>>();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = makeVkStruct();
	pipelineLayoutInfo.flags					= 0;
	pipelineLayoutInfo.setLayoutCount			= data_count(vkLayouts);
	pipelineLayoutInfo.pSetLayouts				= data_or_null(vkLayouts);
	pipelineLayoutInfo.pushConstantRangeCount	= data_count(ranges);
	pipelineLayoutInfo.pPushConstantRanges		= data_or_null(ranges);

	VKASSERTMSG(vkCreatePipelineLayout(*device, &pipelineLayoutInfo, callbacks, pipelineLayout.setter()),
										"failed to create pipeline layout!");

	return pipelineLayout;
}
ZDescriptorSet LayoutManager::getDescriptorSet (ZDescriptorSetLayout layout)
{
	ZDescriptorSet ds = layout.getParam<ZDescriptorSet>();
	ASSERTION(ds.has_handle());
	return ds;
}
VarDescriptorInfo LayoutManager::getDescriptorInfo (uint32_t binding) const
{
	VarDescriptorInfo var;
	const collection_element_t<decltype(m_extbindings)>& b = verifyGetExtBinding(binding);
	if (isBufferDescriptorType(b.descriptorType))
	{
		VkDeviceSize range = ROUNDUP( (b.size * b.elementCount), getDescriptorAlignment(b.descriptorType) );
		auto itBuffer = m_buffers.find({ b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding)) });
		if (m_buffers.cend() == itBuffer)
		{
			ASSERTFALSE("Unable to find anything at binding.",
				" Result from ", __func__, "() is valid after descriptor set is updated");
		}
		var.emplace<DescriptorBufferInfo>(itBuffer->second, b.offset, range);
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
const LayoutManager::ExtBinding& LayoutManager::verifyGetExtBinding (uint32_t binding) const
{
	auto ptr = std::find_if(m_extbindings.begin(), m_extbindings.end(), [&](const auto& b){ return b.binding == binding; });
	ASSERTMSG(ptr != m_extbindings.end(), "Mismatch type binding");
	return *ptr;
}
const LayoutManager::ExtBinding& LayoutManager::verifyGetExtBinding (std::type_index index, uint32_t binding) const
{
	const collection_element_t<decltype(m_extbindings)>& b = verifyGetExtBinding(binding);
	ASSERTMSG(b.index == index, "Mismatch type binding");
	return b;
}
uint32_t LayoutManager::getBindingElementCount (uint32_t binding) const
{
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
	return b.elementCount;
}
void LayoutManager::fillBinding (uint32_t binding, uint32_t value)
{
	uint32_t data[256];
	std::fill(std::begin(data), std::end(data), value);
	add_cref<ExtBinding> b = verifyGetExtBinding(binding);
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
		bufferWriteData(buffer, reinterpret_cast<add_cptr<uint8_t>>(data), writeInfo, false);
	}
	if (orphanSize)
	{
		const VkBufferCopy writeInfo
		{
			0,
			b.offset + chunkCount * ARRAY_LENGTH(data),
			orphanSize,
		};
		bufferWriteData(buffer, reinterpret_cast<add_cptr<uint8_t>>(data), writeInfo, false);
	}
	bufferFlush(buffer);
}
void LayoutManager::writeBinding_ (std::type_index index, uint32_t binding, const uint8_t* data, VkDeviceSize bytes)
{
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	const VkBufferCopy writeInfo { 0, b.offset, std::min((b.size * b.elementCount), bytes) };
	auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	bufferWriteData(buffer, data, writeInfo);
}
void LayoutManager::readBinding_ (std::type_index index, uint32_t binding, uint8_t* data, VkDeviceSize bytes) const
{
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	const VkBufferCopy readInfo { b.offset, 0, std::min((b.size * b.elementCount), bytes) };
	auto buffer = m_buffers.at({b.descriptorType, (b.shared ? UNIQUE_IBINDING : static_cast<int>(binding))});
	bufferReadData(buffer, data, readInfo);
}
void LayoutManager::getBinding_ (uint32_t binding, std::optional<ZImageView>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<ZImageView>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_views[b.offset];
}
void LayoutManager::getBinding_ (uint32_t binding, std::optional<ZSampler>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<ZSampler>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_samplers[b.offset];
}
void LayoutManager::getBinding_ (uint32_t binding, std::optional<std::pair<ZImageView,ZSampler>>& result) const
{
	auto index = std::type_index(typeid(typename add_extent<std::pair<ZImageView,ZSampler>>::type));
	const ExtBinding& b = verifyGetExtBinding(index, binding);
	result = m_viewsAndSamplers[b.offset];
}
void LayoutManager::getBinding_ (uint32_t binding, std::optional<ZBuffer>& result) const
{
	const ExtBinding& b = verifyGetExtBinding(binding);
	const std::pair<VkDescriptorType, int> key(b.descriptorType, make_signed(binding));
	result = m_buffers.at(key);
}

} // namespace vtf

