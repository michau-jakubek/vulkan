#include "vtfBacktrace.hpp"
#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfTemplateUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfFilesystem.hpp"
#include "vtfZBuffer.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfCopyUtils.hpp"
#include "stb_image.hpp"

#include <memory>

namespace vtf
{

static ZBuffer	createBuffer (ZDevice					device,
							  VkDeviceSize				size,
							  type_index_with_default	type,
							  VkFormat					format,
							  add_cref<VkExtent3D>		extent,
							  ZBufferUsageFlags			usage,
							  ZBufferCreateFlags		flags,
							  ZMemoryPropertyFlags		properties)
{
	VkBuffer								handle		= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr				callbacks	= device.getParam<VkAllocationCallbacksPtr>();
	ZPhysicalDevice							phys		= device.getParam<ZPhysicalDevice>();
	add_cref<VkPhysicalDeviceProperties>	pdp			= phys.getParamRef<VkPhysicalDeviceProperties>();
	const bool								sparse		= flags.contain(VK_BUFFER_CREATE_SPARSE_BINDING_BIT);
	const bool								devaddr		= usage.contain(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	if (VkBufferUsageFlags(usage) & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
	{
		ASSERTMSG(size <= pdp.limits.maxStorageBufferRange, "Requested buffer size exceeds maxStorageBufferRange device limits");
	}
	if (VkBufferUsageFlags(usage) & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
	{
		ASSERTMSG(size <= pdp.limits.maxUniformBufferRange, "Requested buffer size exceeds maxUniformBufferRange device limits");
	}
	ASSERTMSG(size != 0u, "Requested buffer size must not be zero");

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.flags		= flags();
	bufferInfo.usage		= VkBufferUsageFlags(usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VKASSERTMSG(vkCreateBuffer(*device, &bufferInfo, callbacks, &handle), "failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*device, handle, &memRequirements);

	auto allocations = createMemory(device, memRequirements, VkMemoryPropertyFlags(properties), size, sparse, devaddr);
	if (false == sparse)
	{
		for (add_ref<ZDeviceMemory> alloc : allocations)
			VKASSERT(vkBindBufferMemory(*device, handle, *alloc, 0));
	}

	return ZBuffer::create(handle, device, callbacks, bufferInfo, allocations, size, type, extent, format);
}

ZDeviceMemory bufferGetMemory (ZBuffer buffer, uint32_t index)
{
	add_ref<std::vector<ZDeviceMemory>> allocations = buffer.getParamRef<std::vector<ZDeviceMemory>>();
	ASSERTMSG((index < data_count(allocations)), "Index out of bounds");
	return allocations.at(index);
}

void bufferBindMemory (ZBuffer buffer, ZQueue sparseQueue)
{
	typedef std::vector<ZDeviceMemory> Allocations;
	ZDevice				device		= buffer.getParam<ZDevice>();
	const VkQueueFlags	queueFlags	= sparseQueue.getParam<ZDistType<QueueFlags, VkQueueFlags>>();
	ASSERTMSG(queueFlags & VkQueueFlags(VK_QUEUE_SPARSE_BINDING_BIT), "Queue is not sparse");
	ASSERTMSG(buffer.getParamRef<VkBufferCreateInfo>().flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT, "Buffer is not sparse");

	VkDeviceSize resourceOffset = 0u;
	add_cref<Allocations> allocations(buffer.getParamRef<Allocations>());
	const typename Allocations::size_type allocCount = allocations.size();
	std::vector<VkSparseMemoryBind> bindings(allocCount);
	for (typename Allocations::size_type a = 0u; a < allocCount; ++a)
	{
		add_cref<ZDeviceMemory>		alloc			(allocations.at(a));
		add_ref<VkSparseMemoryBind> b				(bindings.at(a));
		const VkDeviceSize			allocationSize	= alloc.getParam<VkDeviceSize>();

		b.resourceOffset	= resourceOffset;
		b.size				= allocationSize;
		b.memory			= *alloc;
		b.memoryOffset		= 0u;
		b.flags				= VkSparseMemoryBindFlags(0);

		resourceOffset += allocationSize;
	}

	const VkSparseBufferMemoryBindInfo memoryBindInfo
	{
		*buffer,
		static_cast<uint32_t>(allocCount),
		bindings.data()
	};

	VkStruct<VkBindSparseInfo> bindInfo;
	bindInfo.bufferBindCount	= 1u;
	bindInfo.pBufferBinds		= &memoryBindInfo;

	
	ZFence			fence = createFence(device);
	add_cptr<char>	error = "failed to bind sparse buffer allocations";

	VKASSERTMSG(vkQueueBindSparse(*sparseQueue, 1u, &bindInfo, *fence), error);
	VKASSERTMSG(vkWaitForFences(*device, 1u, fence.ptr(), VK_TRUE, INVALID_UINT64), error);
}

ZBuffer	createTypedBuffer (ZDevice device, type_index_with_default type, VkDeviceSize size,
						   ZBufferUsageFlags usage, ZMemoryPropertyFlags properties, ZBufferCreateFlags flags)
{
	return createBuffer(device, size, type, VK_FORMAT_UNDEFINED, {}, usage, flags, properties);
}

ZBuffer	createBuffer (ZDevice device, VkDeviceSize size,
					ZBufferUsageFlags usage, ZMemoryPropertyFlags properties, ZBufferCreateFlags flags)
{
	return createBuffer(device, size, type_index_with_default(), VK_FORMAT_UNDEFINED, {}, usage, flags, properties);
}

ZBuffer createBuffer (ZDevice device, VkFormat format, uint32_t elements,
					ZBufferUsageFlags usage, ZMemoryPropertyFlags properties, ZBufferCreateFlags flags)
{
	const VkDeviceSize bufferSize = make_unsigned(computePixelByteSize(format)) * elements;
	return createBuffer(device, bufferSize, type_index_with_default::make<VkFormat>(), format, { elements, 0u, 0u }, usage, flags, properties);
}

ZBuffer createIndexBuffer (ZDevice device, uint32_t indexCount, VkIndexType indexType)
{
	uint32_t width = 0u;
	type_index_with_default type;
	if (indexType == VK_INDEX_TYPE_UINT32)
	{
		width = uint32_t(sizeof(uint32_t));
		type = type_index_with_default::make<uint32_t>();
	}
	else if (indexType == VK_INDEX_TYPE_UINT16)
	{
		width = uint32_t(sizeof(uint16_t));
		type = type_index_with_default::make<uint16_t>();
	}
	ASSERTMSG(width, "Unknown index type");
	return createBuffer(device, (indexCount * width), type, VK_FORMAT_UNDEFINED, { indexCount, 0u, 0u },
						VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ZBufferCreateFlags(), ZMemoryPropertyHostFlags);
}

VkDeviceSize	computeBufferSize (VkFormat format, uint32_t imageWidth, uint32_t imageHeight,
								   uint32_t baseMipLevel, uint32_t mipLevelCount, uint32_t layerCount)
{
	const auto [offset, size] = computeMipLevelsOffsetAndSize(format, imageWidth, imageHeight, baseMipLevel, mipLevelCount);
	return (size * layerCount);
}

ZBuffer createBuffer (ZImage image, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties, ZBufferCreateFlags flags)
{
	ASSERTMSG(image.has_handle(), "Image must have a handle");
	add_cref<VkImageCreateInfo> ci = imageGetCreateInfo(image);
	const VkDeviceSize size = computeBufferSize(ci.format, ci.extent.width, ci.extent.height, 0u, ci.mipLevels, ci.arrayLayers);
	return createBuffer(image.getParam<ZDevice>(), size,
						type_index_with_default::make<ZImage>(), ci.format, ci.extent,
						usage, flags, properties);
}

ZBuffer bufferDuplicate (ZBuffer buffer)
{
	add_cref<std::vector<ZDeviceMemory>>	ms		= buffer.getParamRef<std::vector<ZDeviceMemory>>();
	const uint32_t							mc		= data_count(ms);
	ASSERTMSG(mc == 1u, "Duplicate of sparse buffers not implemented yet");

	add_cref<VkBufferCreateInfo>			cinfo	= buffer.getParamRef<VkBufferCreateInfo>();
	VkMemoryPropertyFlags					mprop	= ms.at(0).getParam<VkMemoryPropertyFlags>();

	return createBuffer(buffer.getParam<ZDevice>(),
						cinfo.size,
						buffer.getParamRef< type_index_with_default>(),
						buffer.getParamRef<VkFormat>(),
						buffer.getParamRef<VkExtent3D>(),
						ZBufferUsageFlags::fromFlags(cinfo.usage),
						ZBufferCreateFlags::fromFlags(cinfo.flags),
						ZMemoryPropertyFlags::fromFlags(mprop));
}

void bufferWriteData (ZBuffer buffer, add_cptr<uint8_t> src, add_cref<VkBufferCopy> copy, bool flush)
{
	ZDevice					device = buffer.getParam<ZDevice>();
	ZDeviceMemory			memory = bufferGetMemory(buffer, 0);
	VkMemoryPropertyFlags	props = memory.getParam<VkMemoryPropertyFlags>();
	const VkDeviceSize		bufferSize = buffer.getParam<VkDeviceSize>();

	ASSERTION(copy.size > 0);
	ASSERTION(copy.dstOffset < bufferSize);
	ASSERTION((copy.dstOffset + copy.size) <= bufferSize);

	uint8_t* dst = nullptr;
	VKASSERT(vkMapMemory(*device, *memory, copy.dstOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)));
	ASSERTION(dst != nullptr);

	std::copy(src + copy.srcOffset, src + copy.srcOffset + copy.size, dst);

	if (flush && ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
	{
		VkMappedMemoryRange	range{};
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = *memory;
		range.offset = copy.dstOffset;
		range.size = copy.size;
		VKASSERT(vkFlushMappedMemoryRanges(*device, 1, &range));
	}

	vkUnmapMemory(*device, *memory);
}

VkDeviceSize bufferWriteData (
	ZBuffer			buffer,
	const uint8_t*	src,
	std::size_t		elementSize,	// in bytes
	std::size_t		elementCount,
	uint32_t		dstIndex,		// in elements
	uint32_t		srcIndex,		// in elements
	uint32_t		count)			// in elements
{
	const VkDeviceSize cpySize = count * elementSize;
	const VkDeviceSize srcSize = elementCount * elementSize;
	const VkDeviceSize dstSize = buffer.getParam<VkDeviceSize>();

	VkDeviceSize dstOffset = dstIndex * elementSize;
	VkDeviceSize writeSize = dstOffset < dstSize ? dstSize - dstOffset : 0u;

	VkDeviceSize srcOffset = srcIndex * elementSize;
	VkDeviceSize readSize = srcOffset < srcSize ? srcSize - srcOffset : 0u;

	if (const VkDeviceSize size = std::min(cpySize, std::min(readSize, writeSize)); size > 0u)
	{
		VkBufferCopy copy{};
		copy.srcOffset	= srcOffset;
		copy.dstOffset	= dstOffset;
		copy.size		= size;
		bufferWriteData(buffer, src, copy, true);

		return size;
	}

	return 0u;
}

VkDeviceSize bufferWriteData (ZBuffer buffer, const uint8_t * src, VkDeviceSize size)
{
	const VkDeviceSize              bufferSize = buffer.getParam<VkDeviceSize>();

	if (VK_WHOLE_SIZE == size || bufferSize < size)
		size = bufferSize;

	VkBufferCopy copy{};
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = size;

	bufferWriteData(buffer, src, copy);

	return size;
}

ZBuffer createBufferAndLoadFromImageFile (ZDevice device, add_cref<std::string> imageFileName,
										  ZBufferUsageFlags usage, int desiredChannelCount)
{
	VkFormat	format;
	uint32_t	width, height;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 1>>	sinkWidth	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 2>>	sinkHeight	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 3>>	sinkNcomp	= 0;

	ASSERTION(readImageFileMetadata(imageFileName, format, width, height, desiredChannelCount));

	const uint32_t pixelCount = width * height;

	ZBuffer		buffer = createBuffer(device, format,
									  pixelCount, usage, ZMemoryPropertyHostFlags, ZBufferCreateFlags());
	buffer.setParam<type_index_with_default>(type_index_with_default::make<std::string>());
	add_ref<VkExtent3D> extent = buffer.getParamRef<VkExtent3D>();
	extent.width	= width;
	extent.height	= height;
	extent.depth	= 1u;

	routine_res_t<decltype(stbi_load)> data = stbi_load(imageFileName.c_str(), &sinkWidth, &sinkHeight, &sinkNcomp,
														desiredChannelCount);
	static_assert(std::is_pointer<decltype(data)>::value, "");
	ASSERTION(make_unsigned(sinkWidth) <= width && make_unsigned(sinkHeight) <= height);
	ASSERTION(data);
	std::unique_ptr<stbi_uc, void(*)(routine_res_t<decltype(stbi_load)>)> k(data, [](auto ptr){ stbi_image_free(ptr); });
	add_ptr<uint8_t> ptrData = static_cast<add_ptr<uint8_t>>(static_cast<add_ptr<void>>(k.get()));

	bufferWriteData(buffer, ptrData, VK_WHOLE_SIZE);
	//bufferWriteData(buffer, ptrData, formatGetInfo(format).pixelByteSize, pixelCount, 0u, 0u, INVALID_UINT32);

	return buffer;
}

VkDeviceSize bufferReadData (ZBuffer buffer, uint8_t* dst, VkDeviceSize size)
{
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	if (VK_WHOLE_SIZE == size || bufferSize < size)
		size = bufferSize;

	VkBufferCopy copy{};
	copy.srcOffset	= 0;
	copy.dstOffset	= 0;
	copy.size		= size;

	return bufferReadData(buffer, dst, copy);
}

VkDeviceSize bufferReadData (ZBuffer buffer, uint8_t* dst, const VkBufferCopy& copy)
{
	if (copy.size == 0u) return copy.size;

	ZDeviceMemory			memory		= bufferGetMemory(buffer, 0);
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	ZDevice					device		= buffer.getParam<ZDevice>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTMSG(copy.srcOffset < bufferSize, "Too long data requested to copy from buffer");
	ASSERTMSG((copy.srcOffset + copy.size) <= bufferSize, "Too long data requested to copy from buffer");

	uint8_t* src = nullptr;
	VKASSERT(vkMapMemory(*device, *memory, copy.srcOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&src)));

	if ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.srcOffset;
		range.size		= copy.size;
		VKASSERT(vkInvalidateMappedMemoryRanges(*device, 1, &range));
	}

	std::copy(src, std::next(src, make_signed(copy.size)), dst);
	vkUnmapMemory(*device, *memory);

	return static_cast<uint32_t>(copy.size);
}

void bufferFlush (ZBuffer buffer)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= bufferGetMemory(buffer, 0);
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	if (!(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & props))
	{
		const VkDeviceSize	bufferSize	= buffer.getParam<VkDeviceSize>();

		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= 0;
		range.size		= VK_WHOLE_SIZE;

		void* dst = nullptr;
		VKASSERT(vkMapMemory(*device, *memory, 0, bufferSize, (VkMemoryMapFlags)0, &dst));
		ASSERTION(dst != nullptr);

		VKASSERT(vkFlushMappedMemoryRanges(*device, 1, &range));

		vkUnmapMemory(*device, *memory);
	}
}

VkDeviceSize bufferGetSize (ZBuffer buffer)
{
	return buffer.getParam<VkDeviceSize>();
}

VkDeviceAddress	bufferGetAddress (ZBuffer buffer, uint32_t hintAssertBinding, VkDescriptorType hintAssertDescriptorType)
{
	ASSERTMSG(buffer.has_handle(), "Buffer must have valid handle");
	const VkBufferUsageFlags usage = buffer.getParamRef<VkBufferCreateInfo>().usage;
	auto binding = [&] { return INVALID_UINT32 != hintAssertBinding
		? ("(binding: " + std::to_string(hintAssertBinding) + ", type: "
			+ vk::to_string(static_cast<vk::DescriptorType>(hintAssertDescriptorType)) + ") ") : std::string(" "); };
	ASSERTMSG((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
		"Buffer ", binding(), "was not created with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag");
	VkBufferDeviceAddressInfo info = makeVkStruct();
	info.buffer = *buffer;
	ZDevice device = buffer.getParam<ZDevice>();
	add_cref<ZDeviceInterface> di = device.getInterface();
	VkDeviceAddress addr = 0;
	if (di.vkGetBufferDeviceAddress)
		addr = di.vkGetBufferDeviceAddress(*device, &info);
	else if (di.vkGetBufferDeviceAddressKHR)
		addr = di.vkGetBufferDeviceAddress(*device, &info);
	else if (di.vkGetBufferDeviceAddressEXT)
		addr = di.vkGetBufferDeviceAddressEXT(*device, &info);
	ASSERTMSG(addr, "Device address of buffer must not be zero");
	return addr;
}

VkDeviceSize bufferGetMemorySize	(ZBuffer buffer, uint32_t index)
{
	return bufferGetMemory(buffer, index).getParam<VkDeviceSize>();
}

void bufferCopyToBuffer2 (ZCommandBuffer cmdBuffer, ZBuffer srcBuffer, ZBuffer dstBuffer,
						  ZBarrierConstants::Access srcBufferAccess, ZBarrierConstants::Access dstBufferAccess,
						  ZBarrierConstants::Stage  srcStage,		 ZBarrierConstants::Stage  dstStage)
{
	const VkDeviceSize	srcSize = bufferGetSize(srcBuffer);
	const VkDeviceSize	dstSize = bufferGetSize(dstBuffer);
	ASSERTMSG(srcSize <= dstSize, "Destination buffer must accomodate all data from source buffer");
	ASSERTMSG(!(getGlobalAppFlags().apiVer < Version(1,3)),
			  "Improper Vulkan API version, expected >= 1.3. Try with -api 13");

	VkBufferCopy2		region = makeVkStruct();
	region.srcOffset	= 0u;
	region.dstOffset	= 0u;
	region.size			= srcSize;

	VkCopyBufferInfo2	info = makeVkStruct();
	info.srcBuffer		= *srcBuffer;
	info.dstBuffer		= *dstBuffer;
	info.regionCount	= 1u;
	info.pRegions		= &region;

	using A = ZBarrierConstants::Access;
	using S = ZBarrierConstants::Stage;

	ZBufferMemoryBarrier2	srcBefore(srcBuffer, A::NONE, srcStage,
												 A::TRANSFER_READ_BIT, S::TRANSFER_BIT);
	ZBufferMemoryBarrier2	dstBefore(dstBuffer, A::NONE, srcStage,
												 A::TRANSFER_WRITE_BIT, S::TRANSFER_BIT);
	ZBufferMemoryBarrier2	srcAfter(srcBuffer, A::TRANSFER_READ_BIT, S::TRANSFER_BIT,
												srcBufferAccess, dstStage);
	ZBufferMemoryBarrier2	dstAfter(dstBuffer, A::TRANSFER_WRITE_BIT, S::TRANSFER_BIT,
												dstBufferAccess, dstStage);

	commandBufferPipelineBarriers2(cmdBuffer, srcBefore, dstBefore);
	vkCmdCopyBuffer2(*cmdBuffer, &info);
	commandBufferPipelineBarriers2(cmdBuffer, srcAfter, dstAfter);
}

void bufferCopyToBuffer (ZCommandBuffer cmdBuffer, ZBuffer srcBuffer, ZBuffer dstBuffer,
						 VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
						 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
	const VkDeviceSize	srcSize = bufferGetSize(srcBuffer);
	const VkDeviceSize	dstSize = bufferGetSize(dstBuffer);
	ASSERTMSG(srcSize <= dstSize, "Destination buffer must accomodate all data from source buffer");

	VkBufferCopy region{};
	region.srcOffset	= 0u;
	region.dstOffset	= 0u;
	region.size			= srcSize;

	ZBufferMemoryBarrier	srcBefore(srcBuffer, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT);
	ZBufferMemoryBarrier	dstBefore(dstBuffer, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT);
	ZBufferMemoryBarrier	srcAfter(srcBuffer, VK_ACCESS_TRANSFER_READ_BIT, srcBufferAccess);
	ZBufferMemoryBarrier	dstAfter(dstBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, dstBufferAccess);

	commandBufferPipelineBarriers(cmdBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, srcBefore, dstBefore);
	vkCmdCopyBuffer(*cmdBuffer, *srcBuffer, *dstBuffer, 1u, &region);
	commandBufferPipelineBarriers(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, srcAfter, dstAfter);
}

void bufferCopyToBuffer (ZCommandPool commandPool, ZBuffer srcBuffer, ZBuffer dstBuffer)
{
	auto shotCommand = createOneShotCommandBuffer(commandPool);
	bufferCopyToBuffer(shotCommand->commandBuffer, srcBuffer, dstBuffer,
					  VK_ACCESS_NONE, VK_ACCESS_NONE,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void	bufferCopyToImage	(ZCommandBuffer cmdBuffer, ZBuffer buffer, ZImage image,
							 VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
							 VkAccessFlags srcImageAccess, VkAccessFlags dstImageAccess,
							 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
							 VkImageLayout finalImageLayout,
							 uint32_t baseLevel, uint32_t levelCount,
							 uint32_t baseLayer, uint32_t layerCount)
{
	ASSERTMSG(image.has_handle() && buffer.has_handle(), "Both image and buffer must have a handle");
	add_cref<VkImageCreateInfo>	createInfo			= imageGetCreateInfo(image);
	ASSERTMSG(baseLevel < createInfo.mipLevels, "Mip level must be less than available image mip levels");
	ASSERTMSG(baseLayer < createInfo.arrayLayers, "Layer must be less than available image array layers");
	if ((baseLevel + levelCount) > createInfo.mipLevels) levelCount = 0;
	if ((baseLayer + layerCount) > createInfo.arrayLayers) layerCount = 0;
	const VkDeviceSize			minBufferSize		= imageCalcMipLevelsSize(image, baseLevel, levelCount) * layerCount;
	ASSERTMSG(minBufferSize <= bufferGetSize(buffer), "Buffer must accomodate image data");

	const uint32_t				pixelSize			= make_unsigned(computePixelByteSize(createInfo.format));
	const VkImageAspectFlags	aspect				= formatGetAspectMask(createInfo.format);
	const VkImageLayout			imageSavedLayout	= imageGetLayout(image);

	ZBufferMemoryBarrier		preBufferBarrier	= makeBufferMemoryBarrier(
				buffer, srcBufferAccess, VK_ACCESS_TRANSFER_READ_BIT);
	ZBufferMemoryBarrier		postBufferBarrier	= makeBufferMemoryBarrier(
				buffer,	VK_ACCESS_TRANSFER_READ_BIT, dstBufferAccess);
	ZImageMemoryBarrier			preImageBarrier		= makeImageMemoryBarrier(
				image, srcImageAccess, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	ZImageMemoryBarrier			postImageBarrier	= makeImageMemoryBarrier(
				image, VK_ACCESS_TRANSFER_WRITE_BIT, dstImageAccess,
				(finalImageLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? imageSavedLayout : finalImageLayout);

	VkDeviceSize	bufferOffset	= 0;
	std::vector<VkBufferImageCopy>	regions(layerCount * levelCount);

	for (uint32_t layer = 0; layer < layerCount; ++layer)
	{
		uint32_t	width	= 0u;
		uint32_t	height	= 0u;
		std::tie(width, height) = computeMipLevelWidthAndHeight(createInfo.extent.width, createInfo.extent.height, baseLevel);

		for (uint32_t mipLevel = 0; mipLevel < levelCount; ++mipLevel)
		{
			VkBufferImageCopy		rgn{};
			rgn.bufferOffset		= bufferOffset;
			/*
			 * bufferRowLength and bufferImageHeight specify in texels a subregion
			 * of a larger two- or three-dimensional image in buffer memory,
			 * and control the addressing calculations. If either of these values is zero,
			 * that aspect of the buffer memory is considered to be tightly packed according to the imageExtent
			 */
			rgn.bufferRowLength		= width;
			rgn.bufferImageHeight	= height;
			rgn.imageOffset			= { 0u, 0u, 0u };
			rgn.imageExtent			= { width, height, 1u };
			rgn.imageSubresource.aspectMask		= aspect;
			rgn.imageSubresource.mipLevel		= baseLevel + mipLevel;
			rgn.imageSubresource.baseArrayLayer	= baseLayer + layer;
			rgn.imageSubresource.layerCount		= 1u;

			regions[layer * levelCount + mipLevel] = rgn;

			bufferOffset += (width * height * pixelSize);
			height /= 2;
			width /= 2;
		}
	}

	commandBufferPipelineBarriers(cmdBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, preImageBarrier, preBufferBarrier);
	vkCmdCopyBufferToImage(*cmdBuffer, *buffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uint32_t(regions.size()), regions.data());
	commandBufferPipelineBarriers(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, postImageBarrier, postBufferBarrier);
}


namespace namespace_hidden
{
BufferTexelAccess_::BufferTexelAccess_ (ZBuffer buffer, uint32_t elementSize,
										uint32_t width, uint32_t height, uint32_t depth,
										uint32_t off, VkComponentTypeKHR componentType)
	: m_buffer			(buffer)
	, m_elementSize		(elementSize)
	, m_componentType	(componentType)
	, m_bufferSize		(bufferGetSize(buffer))
	, m_size			(width, height, depth)
	, m_offset			(off)
	, m_data			(nullptr)
{
	ASSERTMSG(width != 0 && height != 0 && depth != 0 && (VkDeviceSize(elementSize) * width * height * depth) <= m_bufferSize,
		"width: ", width, ", height: ", height, ", depth: ", depth, ", elementSize: ", elementSize,
		", ", (VkDeviceSize(elementSize) * width * height * depth), " <= ", m_bufferSize);
	const VkBufferUsageFlags	usage = buffer.getParamRef<VkBufferCreateInfo>().usage;
	ASSERTION(usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const VkMemoryPropertyFlags	flags = bufferGetMemory(buffer, 0u).getParam<VkMemoryPropertyFlags>();
	ASSERTION(flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	VKASSERT(vkMapMemory(*buffer.getParam<ZDevice>(), *buffer.getParam<std::vector<ZDeviceMemory>>().front(),
						  0u, bufferGetMemorySize(buffer), (VkMemoryMapFlags)0, reinterpret_cast<void**>(&m_data)));
}
BufferTexelAccess_::~BufferTexelAccess_ ()
{
	vkUnmapMemory(*m_buffer.getParam<ZDevice>(), *m_buffer.getParam<std::vector<ZDeviceMemory>>().front());
}
add_ptr<void> BufferTexelAccess_::at (uint32_t x, uint32_t y, uint32_t z)
{
	const std::size_t address = static_cast<std::size_t>(m_elementSize * (m_offset + (z * m_size.x() * m_size.y()) + (y * m_size.x()) + x));
	ASSERTION(address < m_bufferSize);
	return &m_data[address];
}
add_cptr<void> BufferTexelAccess_::at (uint32_t x, uint32_t y, uint32_t z) const
{
	const std::size_t address = static_cast<std::size_t>(m_elementSize * (m_offset + (z * m_size.x() * m_size.y()) + (y * m_size.x()) + x));
	ASSERTION(address < m_bufferSize);
	return &m_data[address];
}

template<class T> float convertToFloat (void_cptr p)
{
	return float(double(*((const T*)(p))) / double(std::numeric_limits<T>::max()));
}

float convertToFloat (const uint32_t component, add_cref<ZFormatInfo> info, void_cptr p)
{
	ASSERTION(info.pack > 0 && info.pack <= 64);

	uint32_t shift = 0;
	uint64_t color = 0;
	std::memcpy(&color, p, (info.pack / 8));

	std::array<uint32_t, 4> reordered;
	reordered.fill(INVALID_UINT32);
	for (uint32_t c = 0; c < info.componentCount; ++c)
	{
		const auto sw = info.swizzling[c];
		if (sw >= 0)
			reordered[3u - uint32_t(sw)] = c;
	}

	for (uint32_t r = 0; r < info.componentCount; ++r)
	{
		const uint32_t c = reordered[r];
		if (c == INVALID_UINT32) continue;
		const auto bw = info.componentBitSizes[c];
		const uint64_t mask = (1ULL << bw) - 1;
		const uint64_t value = (color >> shift) & mask;
		shift += bw;

		if (component == c)
		{
			const int den = (1 << (info.isSigned ? (bw - 1) : bw)) - 1;
			if (den == 0) break;
			const float nom = float(value);
			const float res = nom / float(den);
			return res;
		}
	}

	return 0.0f;
}

Vec4 BufferTexelAccess_::asColor (VkFormat format, uint32_t x, uint32_t y, uint32_t z) const
{
	if (m_componentType == VK_COMPONENT_TYPE_MAX_ENUM_KHR)
	{
		return {};
	}

	Vec4 result;
	void_cptr p = at(x, y, z);

	const ZFormatInfo fi = formatGetInfo(format);
	if (fi.pack > 0)
	{
		result.r(convertToFloat(0, fi, p));
		result.g(convertToFloat(1, fi, p));
		result.b(convertToFloat(2, fi, p));
		result.a(convertToFloat(3, fi, p));
		return result;
	}

	const uint32_t componentCount = m_elementSize / getComponentByteSize(m_componentType);

	for (uint32_t c = 0; c < componentCount; ++c)
	{
		switch (m_componentType)
		{
		case VK_COMPONENT_TYPE_SINT8_KHR:
			result[c] = convertToFloat<int8_t>(p);
			break;
		case VK_COMPONENT_TYPE_UINT8_KHR:
			result[c] = convertToFloat<uint8_t>(p);
			break;
		case VK_COMPONENT_TYPE_FLOAT16_KHR:
			result[c] = float16ToFloat32(Float16::construct(*((const uint16_t*)(p))));
			break;
		case VK_COMPONENT_TYPE_SINT16_KHR:
			result[c] = convertToFloat<int16_t>(p);
			break;
		case VK_COMPONENT_TYPE_UINT16_KHR:
			result[c] = convertToFloat<uint16_t>(p);
			break;
		case VK_COMPONENT_TYPE_FLOAT32_KHR:
			result[c] = *((const float*)(p));
			break;
		case VK_COMPONENT_TYPE_SINT32_KHR:
			result[c] = convertToFloat<int32_t>(p);
			break;
		case VK_COMPONENT_TYPE_UINT32_KHR:
			result[c] = convertToFloat<uint32_t>(p);
			break;
		case VK_COMPONENT_TYPE_FLOAT64_KHR:
			result[c] = float(*((const double*)(p)));
			break;
		case VK_COMPONENT_TYPE_SINT64_KHR:
			result[c] = convertToFloat<int64_t>(p);
			break;
		case VK_COMPONENT_TYPE_UINT64_KHR:
			result[c] = convertToFloat<uint64_t>(p);
			break;
		default: break;
		}
	}

	return result;
}

} // namespace_hidden

} // namespace vtf
