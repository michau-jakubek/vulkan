#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
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
							  ZMemoryPropertyFlags		properties)
{
	VkBuffer								buffer		= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr				callbacks	= device.getParam<VkAllocationCallbacksPtr>();
	ZPhysicalDevice							phys		= device.getParam<ZPhysicalDevice>();
	add_cref<VkPhysicalDeviceProperties>	pdp			= phys.getParamRef<VkPhysicalDeviceProperties>();

	if (VkBufferUsageFlags(usage) & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
	{
		ASSERTMSG(size <= pdp.limits.maxStorageBufferRange, "Requested buffer size exceeds maxStorageBufferRange device limits");
	}

	if (VkBufferUsageFlags(usage) & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
	{
		ASSERTMSG(size <= pdp.limits.maxUniformBufferRange, "Requested buffer size exceeds maxUniformBufferRange device limits");
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.usage		= VkBufferUsageFlags(usage) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VKASSERT(vkCreateBuffer(*device, &bufferInfo, callbacks, &buffer), "failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*device, buffer, &memRequirements);

	ZDeviceMemory	bufferMemory = createMemory(device, memRequirements, VkMemoryPropertyFlags(properties));

	VKASSERT2(vkBindBufferMemory(*device, buffer, *bufferMemory, 0));

	return ZBuffer::create(buffer, device, callbacks, bufferInfo, bufferMemory, size, type, extent, format);
}

ZBuffer	createBuffer (ZDevice device, VkDeviceSize size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	return createBuffer(device, size, type_index_with_default(), VK_FORMAT_UNDEFINED, {}, usage, properties);
}

ZBuffer createBuffer (ZDevice device, VkFormat format, uint32_t elements, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const VkDeviceSize bufferSize = make_unsigned(computePixelByteSize(format)) * elements;
	return createBuffer(device, bufferSize, type_index_with_default::make<VkFormat>(), format, { elements, 0u, 0u }, usage, properties);
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
						VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ZMemoryPropertyHostFlags);
}

VkDeviceSize	computeBufferSize (VkFormat format, uint32_t imageWidth, uint32_t imageHeight,
								   uint32_t baseMipLevel, uint32_t mipLevelCount, uint32_t layerCount)
{
	const auto [offset, size] = computeMipLevelsOffsetAndSize(format, imageWidth, imageHeight, baseMipLevel, mipLevelCount);
	return (size * layerCount);
}

ZBuffer createBuffer (ZImage image, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	ASSERTMSG(image.has_handle(), "Image must have a handle");
	add_cref<VkImageCreateInfo> ci = imageGetCreateInfo(image);
	const VkDeviceSize size = computeBufferSize(ci.format, ci.extent.width, ci.extent.height, 0u, ci.mipLevels, ci.arrayLayers);
	return createBuffer(image.getParam<ZDevice>(), size,
						type_index_with_default::make<ZImage>(), ci.format, ci.extent,
						usage, properties);
}

ZBuffer bufferDuplicate (ZBuffer buffer)
{
	ASSERTMSG(buffer.has_handle(), "Buffer handle must not be VK_NULL_HANDLE");

	ZDevice							dev		= buffer.getParam<ZDevice>();
	add_cref<VkBufferCreateInfo>	cinfo	= buffer.getParamRef<VkBufferCreateInfo>();
	VkMemoryPropertyFlags			mprop	= buffer.getParam<ZDeviceMemory>().getParam<VkMemoryPropertyFlags>();

	return createBuffer(dev, cinfo.size, ZBufferUsageFlags::fromFlags(cinfo.usage), ZMemoryPropertyFlags::fromFlags(mprop));
}

ZBuffer createBufferAndLoadFromImageFile (ZDevice device, add_cref<std::string> imageFileName,
										  ZBufferUsageFlags usage, bool forceFourComponentFormat)
{
	VkFormat	format;
	uint32_t	width, height;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 1>>	sinkWidth	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 2>>	sinkHeight	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_load), 3>>	sinkNcomp	= 0;

	ASSERTION(readImageFileMetadata(imageFileName, format, width, height));

	const uint32_t pixelCount = width * height;

	ZBuffer		buffer = createBuffer(device,
									  (forceFourComponentFormat ? VK_FORMAT_R8G8B8A8_UNORM : format),
									  pixelCount, usage, ZMemoryPropertyHostFlags);
	buffer.setParam<type_index_with_default>(type_index_with_default::make<std::string>());
	add_ref<VkExtent3D> extent = buffer.getParamRef<VkExtent3D>();
	extent.width	= width;
	extent.height	= height;
	extent.depth	= 1u;

	routine_res_t<decltype(stbi_load)> data = stbi_load(imageFileName.c_str(), &sinkWidth, &sinkHeight, &sinkNcomp,
														forceFourComponentFormat ? 4 : 0);
	static_assert(std::is_pointer<decltype(data)>::value, "");
	ASSERTION(make_unsigned(sinkWidth) <= width && make_unsigned(sinkHeight) <= height);
	ASSERTION(data);
	std::unique_ptr<stbi_uc, void(*)(routine_res_t<decltype(stbi_load)>)> k(data, [](auto ptr){ stbi_image_free(ptr); });
	add_ptr<uint8_t> ptrData = static_cast<add_ptr<uint8_t>>(static_cast<add_ptr<void>>(k.get()));
#if 0
	if ((format != VK_FORMAT_R8G8B8A8_UNORM) && forceFourComponentFormat)
	{
		PixelBufferAccess<BVec4> pixels(buffer, width, height, 1u);
		add_ptr<BVec3> fakeSrc3 = static_cast<add_ptr<BVec3>>(static_cast<add_ptr<void>>(k.get()));
		add_ptr<BVec2> fakeSrc2 = static_cast<add_ptr<BVec2>>(static_cast<add_ptr<void>>(k.get()));
		add_ptr<BVec1> fakeSrc1 = static_cast<add_ptr<BVec1>>(static_cast<add_ptr<void>>(k.get()));

		for (uint32_t i = 0; i < pixelCount; ++i)
		{
			switch (sinkNcomp)
			{
			case 3: pixels.at(i, 0u, 0u) = BVec4(fakeSrc3[i].r(), fakeSrc3[i].g(), fakeSrc3[i].b(), 255u); break;
			case 2: pixels.at(i, 0u, 0u) = BVec4(fakeSrc2[i].r(), fakeSrc2[i].g(), 0u, 255u); break;
			case 1: pixels.at(i, 0u, 0u) = BVec4(fakeSrc1[i].r(), 0u, 0u, 255u); break;
			default: ASSERTION(false);
			}
		}
	}
	else
#endif
	bufferWriteData(buffer, ptrData, VK_WHOLE_SIZE);

	return buffer;
}

VkDeviceSize bufferWriteData (ZBuffer buffer, const uint8_t* src, VkDeviceSize size)
{
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	if (VK_WHOLE_SIZE == size || bufferSize < size)
		size = bufferSize;

	VkBufferCopy copy{};
	copy.srcOffset	= 0;
	copy.dstOffset	= 0;
	copy.size		= size;

	return bufferWriteData(buffer, src, copy);
}

VkDeviceSize bufferWriteData (ZBuffer buffer, const uint8_t* src, const VkBufferCopy& copy, bool flush)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTION(copy.size > 0);
	ASSERTION(copy.dstOffset < bufferSize);
	ASSERTION((copy.dstOffset + copy.size) <= bufferSize);

	uint8_t* dst = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.dstOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)));
	ASSERTION(dst != nullptr);

	std::copy(src, std::next(src, make_signed(copy.size)), dst);

	if (flush && ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.dstOffset;
		range.size		= copy.size;
		VKASSERT2(vkFlushMappedMemoryRanges(*device, 1, &range));
	}

	vkUnmapMemory(*device, *memory);

	return (copy.size);
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

	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	ZDevice					device		= buffer.getParam<ZDevice>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTMSG(copy.srcOffset < bufferSize, "Too long data requested to copy from buffer");
	ASSERTMSG((copy.srcOffset + copy.size) <= bufferSize, "Too long data requested to copy from buffer");

	uint8_t* src = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.srcOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&src)));

	if ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.srcOffset;
		range.size		= copy.size;
		VKASSERT2(vkInvalidateMappedMemoryRanges(*device, 1, &range));
	}

	std::copy(src, std::next(src, make_signed(copy.size)), dst);
	vkUnmapMemory(*device, *memory);

	return static_cast<uint32_t>(copy.size);
}

void bufferFlush (ZBuffer buffer)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
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
		VKASSERT2(vkMapMemory(*device, *memory, 0, bufferSize, (VkMemoryMapFlags)0, &dst));
		ASSERTION(dst != nullptr);

		VKASSERT(vkFlushMappedMemoryRanges(*device, 1, &range), "");

		vkUnmapMemory(*device, *memory);
	}
}

VkDeviceSize bufferGetSize (ZBuffer buffer)
{
	return buffer.getParam<VkDeviceSize>();
}

VkDeviceSize bufferGetMemorySize	(ZBuffer buffer)
{
	return buffer.getParam<ZDeviceMemory>().getParam<VkDeviceSize>();
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
	region.dstOffset	= 0;
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
BufferTexelAccess_::BufferTexelAccess_ (ZBuffer buffer, uint32_t elementSize, uint32_t width, uint32_t height, uint32_t depth)
	: m_buffer		(buffer)
	, m_elementSize	(elementSize)
	, m_bufferSize	(bufferGetSize(buffer))
	, m_size		(width, height, depth)
	, m_data		(nullptr)
{
	ASSERTION(width != 0 && height != 0 && depth != 0 && (VkDeviceSize(elementSize) * width * height * depth) <= m_bufferSize);
	const VkBufferUsageFlags	usage = buffer.getParamRef<VkBufferCreateInfo>().usage;
	ASSERTION(usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const VkMemoryPropertyFlags	flags = buffer.getParam<ZDeviceMemory>().getParam<VkMemoryPropertyFlags>();
	ASSERTION(flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	VKASSERT2(vkMapMemory(*buffer.getParam<ZDevice>(), *buffer.getParam<ZDeviceMemory>(),
						  0u, bufferGetMemorySize(buffer), (VkMemoryMapFlags)0, reinterpret_cast<void**>(&m_data)));
}
BufferTexelAccess_::~BufferTexelAccess_ () { vkUnmapMemory(*m_buffer.getParam<ZDevice>(), *m_buffer.getParam<ZDeviceMemory>()); }
add_ptr<void> BufferTexelAccess_::at (uint32_t x, uint32_t y, uint32_t z)
{
	const std::size_t address = static_cast<std::size_t>(m_elementSize * ((z * m_size.x() * m_size.y()) + (y * m_size.x()) + x));
	ASSERTION(address < m_bufferSize);
	return &m_data[address];
}
add_cptr<void> BufferTexelAccess_::at (uint32_t x, uint32_t y, uint32_t z) const
{
	const std::size_t address = static_cast<std::size_t>(m_elementSize * ((z * m_size.x() * m_size.y()) + (y * m_size.x()) + x));
	ASSERTION(address < m_bufferSize);
	return &m_data[address];
}

} // namespace_hidden

} // namespace vtf
