#include "vtfVkUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfFilesystem.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfBacktrace.hpp"
#include "stb_image.hpp"
#include <algorithm>

namespace vtf
{

VkSamplerCreateInfo makeSamplerCreateInfo(VkSamplerAddressMode	wrapS,
										  VkSamplerAddressMode	wrapT,
										  VkFilter				minFilter,
										  VkFilter				magFilter,
										  bool					normalized)
{
	VkSamplerCreateInfo info{};
	info.sType				= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext				= nullptr;
	info.flags				= static_cast<VkSamplerCreateFlags>(0);
	info.magFilter			= magFilter;
	info.minFilter			= minFilter;
	info.addressModeU		= wrapS;
	info.addressModeV		= wrapT;
	info.addressModeW		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	info.mipLodBias			= 0.0f;
	info.anisotropyEnable	= VK_FALSE;
	info.maxAnisotropy		= 1.0f; // ignored if anisotropyEnable is false
	info.compareEnable		= VK_FALSE;
	info.compareOp			= VK_COMPARE_OP_NEVER;
	info.minLod				= 0.0f;
	info.maxLod				= 1024.0f;	// to disable mipmaping use 0.225 if normalized or 0 otherwise
	info.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	info.unnormalizedCoordinates	= normalized ? VK_FALSE : VK_TRUE;
	return info;
}

ZSampler createSampler (ZDevice device, VkFormat format, uint32_t mipLevels, bool filterLinearORnearest, bool normalized, bool mipMapEnable, bool anisotropyEnable)
{
	const ZPhysicalDevice			physDevice	= device.getParam<ZPhysicalDevice>();
	const VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();
	VkFilter						filter		= VK_FILTER_NEAREST;
	VkSamplerMipmapMode				mipMapMode	= VK_SAMPLER_MIPMAP_MODE_NEAREST;

	if (filterLinearORnearest)
	{
		VkFormatProperties				formatProperties;
		// test if image format supports linear blitting
		vkGetPhysicalDeviceFormatProperties(*physDevice, format, &formatProperties);
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
		{
			filter		= VK_FILTER_LINEAR;
			mipMapMode	= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
		else
		{
			ASSERTMSG(0, "Linear filtering is not supported for that format");
		}
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType					= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.pNext					= nullptr;
	if (mipMapEnable)
	{
		samplerInfo.minLod				= 0.0f;
		samplerInfo.maxLod				= static_cast<float>(mipLevels);
		samplerInfo.mipLodBias			= 0.0f;
	}
	else
	{
		samplerInfo.minLod				= 0.0f;
		samplerInfo.maxLod				= normalized ? 0.225f : 0.0f;
		samplerInfo.mipLodBias			= 0.0f;
	}
	samplerInfo.addressModeU			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW			= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.borderColor				= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	if (anisotropyEnable)
	{
		samplerInfo.anisotropyEnable	= VK_TRUE;
		samplerInfo.maxAnisotropy		= physDevice.getParamRef<VkPhysicalDeviceProperties>().limits.maxSamplerAnisotropy;
	}
	else
	{
		samplerInfo.anisotropyEnable	= VK_FALSE;
		samplerInfo.maxAnisotropy		= 0.0f;
	}
	samplerInfo.magFilter				= filter;
	samplerInfo.minFilter				= filter;
	samplerInfo.mipmapMode				= mipMapMode;
	samplerInfo.compareEnable			= VK_FALSE;
	samplerInfo.compareOp				= VK_COMPARE_OP_ALWAYS;
	samplerInfo.unnormalizedCoordinates	= normalized ? VK_FALSE : VK_TRUE;

	ZSampler sampler(VK_NULL_HANDLE, device, callbacks, samplerInfo);
	VKASSERT3(vkCreateSampler(*device, &samplerInfo, callbacks, sampler.setter()), "Failed to create sampler");

	return sampler;
}

ZSampler createSampler (ZImageView view, bool filterLinearORnearest, bool normalized, bool mipMapEnable, bool anisotropyEnable)
{
	const ZDevice	device		= view .getParam<ZDevice>();
	const VkFormat	format		= view.getParamRef<VkImageViewCreateInfo>().format;
	const uint32_t	mipLevels	= view.getParam<ZImage>().getParamRef<VkImageCreateInfo>().mipLevels;
	return createSampler(device, format, mipLevels, filterLinearORnearest, normalized, mipMapEnable, anisotropyEnable);
}

ZImage createImage (ZDevice device, VkFormat format, VkImageType type, uint32_t width, uint32_t height,
					ZImageUsageFlags usage, VkSampleCountFlagBits samples,
					uint32_t mipLevels, uint32_t layers, uint32_t depth, ZMemoryPropertyFlags properties)
{
	const auto availableLevels	= computeMipLevelCount(width, height);
	const auto effectiveUsage	= VkImageUsageFlags(usage) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const auto effectiveLevels	= (INVALID_UINT32 == mipLevels) ? availableLevels : mipLevels ? std::min(mipLevels, availableLevels) : 1;
	const auto callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	const VkImageTiling			tiling	= VK_IMAGE_TILING_OPTIMAL;
	const VkImageCreateFlags	flags	= (layers == 6u) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : VkImageCreateFlagBits(0);
	VkImageFormatProperties		props	{};
	ZPhysicalDevice phys = device.getParam<ZPhysicalDevice>();
	if (getGlobalAppFlags().verbose)
	{
		const char newLine = '\n';
		ZFormatInfo formatInfo = formatGetInfo(format);
		std::cout << "[INFO] trying to create as image with:\n"
				  << "       Format: " << formatInfo.name << newLine
				  << "       Width:  " << width << newLine
				  << "       Height: " << height << newLine
				  << "       Depth:  " << depth << newLine
				  << "       Levels: " << mipLevels << newLine
				  << "       Layers: " << layers
				  << std::endl;
	}
	const VkResult				status = vkGetPhysicalDeviceImageFormatProperties(*phys, format, type, tiling, effectiveUsage, flags, &props);
	VKASSERT3(status, "Unable to create an image with specified parameters");
	// TODO:
	// VkExtent3D            maxExtent;
	// uint32_t              maxMipLevels;
	// uint32_t              maxArrayLayers;
	// VkSampleCountFlags    sampleCounts;
	// VkDeviceSize          maxResourceSize;


	VkImageCreateInfo imageInfo{};
	imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext			= nullptr;
	imageInfo.flags			= flags;
	imageInfo.imageType		= type;
	imageInfo.format		= format;
	imageInfo.extent.width	= width;
	imageInfo.extent.height	= height;
	imageInfo.extent.depth	= depth;
	imageInfo.mipLevels		= effectiveLevels;
	imageInfo.arrayLayers	= layers;
	imageInfo.samples		= samples;
	imageInfo.tiling		= tiling,
	imageInfo.usage			= effectiveUsage;
	imageInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VkImage	image = VK_NULL_HANDLE;
	VKASSERT2(vkCreateImage(*device, &imageInfo, callbacks, &image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, image, &memRequirements);

	auto allocations = createMemory(device, memRequirements, properties(), memRequirements.size, false);

	vkBindImageMemory(*device, image, *allocations.at(0), 0);

	return ZImage::create(image, device, callbacks, imageInfo, allocations.at(0), memRequirements.size);
}

ZNonDeletableImage::ZNonDeletableImage () : ZImage()
{
	super::get()->routine = nullptr;
}

ZNonDeletableImage:: ZNonDeletableImage (VkImage image, add_cref<ZDevice> device, add_cref<VkImageCreateInfo> info, add_cref<VkDeviceSize> size)
	: ZImage(image, device, device.getParam<VkAllocationCallbacksPtr>(), info, ZDeviceMemory(), size)
{
	super::get()->routine = nullptr;
}

ZNonDeletableImage ZNonDeletableImage::create (VkImage image, ZDevice device,
											   VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
											   uint32_t mipLevels, uint32_t layers, VkSampleCountFlagBits samples)
{
	const auto availableLevels	= computeMipLevelCount(width, height);
	const auto effectiveLevels	= (INVALID_UINT32 == mipLevels) ? availableLevels : mipLevels ? std::min(mipLevels, availableLevels) : 1;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext			= nullptr;
	imageInfo.flags			= (layers == 6u) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : VkImageCreateFlagBits(0);
	imageInfo.imageType		= VK_IMAGE_TYPE_2D;
	imageInfo.format		= format;
	imageInfo.extent.width	= width;
	imageInfo.extent.height	= height;
	imageInfo.extent.depth	= 1u;
	imageInfo.mipLevels		= effectiveLevels;
	imageInfo.arrayLayers	= layers;
	imageInfo.samples		= samples;
	imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL,
	imageInfo.usage			= usage();
	imageInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	ASSERTMSG(image != VK_NULL_HANDLE, "Image handle must not be null");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, image, &memRequirements);

	return ZNonDeletableImage(image, device, imageInfo, memRequirements.size);
}

bool readImageFileMetadata (add_cref<std::string> fileName, add_ref<VkFormat> format,
							add_ref<uint32_t> width, add_ref<uint32_t> height)
{
	bool result = false;
	if (fs::exists(fs::path(fileName)))
	{
		result = true;

		int ncomp = 0;

		stbi_info(fileName.c_str(), (int32_t*)&width, (int32_t*)&height, (int32_t*)&ncomp);

		switch (ncomp)
		{
		case 1:	format = VK_FORMAT_R8_UNORM;		break;
		case 2: format = VK_FORMAT_R8G8_UNORM;		break;
		case 3: format = VK_FORMAT_R8G8B8_UNORM;	break;
		case 4: format = VK_FORMAT_R8G8B8A8_UNORM;	break;
		default:	result = false;
		}
	}
	return result;
}

ZImage createImageFromFileMetadata	(ZDevice device, add_cref<std::string> fileName,
									 ZImageUsageFlags usage, ZMemoryPropertyFlags memory,
									 bool forceFourComponentFormat)
{
	uint32_t	imageWidth	= 0;
	uint32_t	imageHeight	= 0;
	VkFormat	imageFormat	= VK_FORMAT_UNDEFINED;
	ASSERTION(readImageFileMetadata(fileName, imageFormat, imageWidth, imageHeight));
	ZImage image = createImage(device,
							   (forceFourComponentFormat ? VK_FORMAT_R8G8B8A8_UNORM : imageFormat),
							   VK_IMAGE_TYPE_2D, imageWidth, imageHeight, usage, VK_SAMPLE_COUNT_1_BIT, 1u, 1u, 1u, memory);
	return image;
}

ZImage createImageFromFileMetadata	(ZDevice device, ZBuffer imageContentBuffer,
									 ZImageUsageFlags usage, ZMemoryPropertyFlags memory)
{
	ASSERTMSG(imageContentBuffer.getParam<type_index_with_default>() == type_index_with_default::make<std::string>(),
			  "Buffer must be created from a image file");
	const uint32_t		imageWidth	= imageContentBuffer.getParamRef<VkExtent3D>().width;
	const uint32_t		imageHeight	= imageContentBuffer.getParamRef<VkExtent3D>().height;
	const VkFormat		imageFormat	= imageContentBuffer.getParam<VkFormat>();
	const VkDeviceSize	bufferSize	= imageContentBuffer.getParam<VkDeviceSize>();
	ASSERTMSG(computeBufferSize(imageFormat, imageWidth, imageHeight) == bufferSize,
			  "Buffer must be created from a image file");
	return createImage(device, imageFormat, VK_IMAGE_TYPE_2D, imageWidth, imageHeight,
					   usage, VK_SAMPLE_COUNT_1_BIT, 1u, 1u, 1u, memory);
}

static bool adjustImageFileSizes (const strings& fileNames, const uint32_t minFileCount,
								  add_ref<uint32_t> width, add_ref<uint32_t> height, add_ref<uint32_t> ncomp)
{
	ASSERTION(fileNames.size() >= minFileCount);

	using		width_t		= std::remove_pointer_t<routine_arg_t<decltype(stbi_info), 1>>;
	using		height_t	= std::remove_pointer_t<routine_arg_t<decltype(stbi_info), 2>>;
	using		ncomp_t		= std::remove_pointer_t<routine_arg_t<decltype(stbi_info), 3>>;

	width_t		x			= std::numeric_limits<width_t>::max();
	height_t	y			= std::numeric_limits<height_t>::max();
	ncomp_t		k			= std::numeric_limits<ncomp_t>::max();
	bool		filesExist	= false;
	uint32_t	i			= 0;

	do
	{
		const fs::path filePath(fileNames[i]);
		filesExist = fs::exists(filePath);
		if (filesExist)
		{
			width_t		w = 0;
			height_t	h = 0;
			ncomp_t		c = 0;
			stbi_info(fileNames[i].c_str(), &w, &h, &c);
			if (i)
				filesExist = (k == c);
			else k = c;
			x = std::min(x, w);
			y = std::min(y, h);
		}
	}
	while (filesExist && ++i < minFileCount);

	width	= make_unsigned(x);
	height	= make_unsigned(y);
	ncomp	= make_unsigned(k);

	return filesExist;
}

ZImage createCubeImageAndLoadFromFiles	(ZDevice device, ZCommandPool commandPool, const strings& fileNames,
										 ZImageUsageFlags usage, VkImageAspectFlags aspect,
										 VkImageLayout finalLayout, VkAccessFlags finalAccess)
{
	ASSERTMSG(fileNames.size() >= 6, "For cube image the file count must be equal or greater than 6");

	uint32_t				width		= 0;
	uint32_t				height		= 0;
	uint32_t				ncomp		= 0;
	ASSERTION(adjustImageFileSizes(fileNames, 6u, width, height, ncomp));

	std::vector<ZBuffer>	buffers;
	const uint32_t			pixelCount	= (width * height);
	static_assert(std::is_same<std::remove_pointer_t<routine_res_t<decltype(stbi_loadf)>>, float>::value, "");
	const VkDeviceSize		bufferSize	= pixelCount * ncomp * sizeof(std::remove_pointer_t<routine_res_t<decltype(stbi_loadf)>>);
	VkFormat				dataFormat	= VK_FORMAT_UNDEFINED;

	switch (ncomp)
	{
	case 1:	dataFormat = VK_FORMAT_R32_SFLOAT;			break;
	case 2: dataFormat = VK_FORMAT_R32G32_SFLOAT;		break;
	case 3: dataFormat = VK_FORMAT_R32G32B32_SFLOAT;	break;
	case 4: dataFormat = VK_FORMAT_R32G32B32A32_SFLOAT;	break;
	default:	ASSERTION(false);
	}

	for (uint32_t layer = 0; layer < 6; ++layer)
	{
		routine_res_t<decltype(stbi_loadf)>								data		{};
		std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 1>>	sinkWidth	= 0;
		std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 2>>	sinkHeight	= 0;
		std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 3>>	sinkNcomp	= 0;

		data = stbi_loadf(fileNames[layer].c_str(), &sinkWidth, &sinkHeight, &sinkNcomp, 4);

		ASSERTION(data);
		ASSERTION(make_signed(width) <= sinkWidth);
		ASSERTION(make_signed(height) <= sinkHeight);
		ASSERTION(make_signed(ncomp) == sinkNcomp);

		ZBuffer buffer = createBuffer	(device, dataFormat, pixelCount,
										 ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
										 ZMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
										 ZBufferCreateFlags());

//#define MANUAL_IMAGE
#ifdef MANUAL_IMAGE
		Vec4 colors[6] =
		{
			  { 1,0,0,1 }
			, { 0,1,0,1 }
			, { 0,0,1,1 }
			, { 1,1,0,1 }
			, { 1,0,1,1 }
			, { 0,1,1,1 }
		};

		if (layer)
		{
			std::vector<Vec4> vv(pixelCount, colors[layer]);
			for (uint32_t h = 0; h < height; ++h)
			for (uint32_t w = 0; w < width; ++w)
			{
				if ((h > height/2 && h < height-30 && w > width/2 && w < width-30) || (h < 30 && w < 30))
				//if (h > height/2 && w > width/2)
				{
						vv.at(h*width+w) = colors[layer-1];
				}
			}
			writeBufferData(buffer, reinterpret_cast<add_ptr<uint8_t>>(vv.data()), bufferSize);
		}
		else
#endif
		{
			bufferWriteData(buffer, reinterpret_cast<add_ptr<uint8_t>>(data), bufferSize);
		}
		stbi_image_free(data);
		buffers.emplace_back(buffer);
	}

	ZImage image = createImage(device, dataFormat, VK_IMAGE_TYPE_2D, width, height, usage,
							   VK_SAMPLE_COUNT_1_BIT, 1u, 6u, 1u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	{
		auto oneShotCommand	= createOneShotCommandBuffer(commandPool);

		VkBufferImageCopy region{};
		region.bufferOffset					= 0;
		region.bufferRowLength				= 0;
		region.bufferImageHeight			= 0;
		region.imageSubresource.aspectMask	= aspect;
		region.imageSubresource.mipLevel	= 0;
		region.imageSubresource.layerCount	= 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = { width, height, 1u};

		ZImageMemoryBarrier before(image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		commandBufferPipelineBarriers(oneShotCommand->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, before);

		for (uint32_t layer = 0; layer < 6; ++layer)
		{
			region.imageSubresource.baseArrayLayer = layer;
			vkCmdCopyBufferToImage(*oneShotCommand->commandBuffer, *buffers[layer], *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
		}

		ZImageMemoryBarrier after(image, VK_ACCESS_TRANSFER_WRITE_BIT, finalAccess, finalLayout);
		commandBufferPipelineBarriers(oneShotCommand->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, after);
	}

	return image;
}

VkImageViewType imageTypeToViewType (VkImageType imageType)
{
	VkImageViewType res = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	switch (imageType)
	{
	case VK_IMAGE_TYPE_1D:	res = VK_IMAGE_VIEW_TYPE_1D;	break;
	case VK_IMAGE_TYPE_2D:	res = VK_IMAGE_VIEW_TYPE_2D;	break;
	case VK_IMAGE_TYPE_3D:	res = VK_IMAGE_VIEW_TYPE_3D;	break;
	default: ASSERTION(false);
	}
	return res;
}

ZImageView createImageView (ZImage image, uint32_t baseMipLevel, uint32_t mipLevels,
							uint32_t baseArrayLayer, uint32_t arrayLayers, VkFormat format,
							VkImageViewType viewType, VkImageAspectFlags aspect)

{
	VkImageView					imageView	= VK_NULL_HANDLE;
	add_cref<VkImageCreateInfo>	imageInfo	= image.getParam<VkImageCreateInfo>();
	ZDevice						device		= image.getParam<ZDevice>();
	VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();

	ASSERTION((arrayLayers != 0) && (baseArrayLayer < imageInfo.arrayLayers) && ((baseArrayLayer + arrayLayers) <= imageInfo.arrayLayers));
	ASSERTION((mipLevels != 0) && (baseMipLevel < imageInfo.mipLevels) && ((baseMipLevel + mipLevels) <= imageInfo.mipLevels));

	const VkFormat				applyFormat	= (VK_FORMAT_UNDEFINED == format) ? imageInfo.format : format;
	const VkImageAspectFlags	applyAspect	= (VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM == aspect) ? formatGetAspectMask(applyFormat) : aspect;
	const VkImageViewType		applyType	= (VK_IMAGE_VIEW_TYPE_MAX_ENUM != viewType)
												? viewType
												: ((imageInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT))
													&& (0u == baseArrayLayer) && (6u == arrayLayers)
												  ? VK_IMAGE_VIEW_TYPE_CUBE
												  : imageTypeToViewType(imageInfo.imageType);

	VkComponentMapping components{};
	components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask		= applyAspect;
	subresourceRange.baseMipLevel	= baseMipLevel;
	subresourceRange.levelCount		= mipLevels;
	subresourceRange.baseArrayLayer	= baseArrayLayer;
	subresourceRange.layerCount		= arrayLayers;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image				= *image;
	viewInfo.viewType			= applyType;
	viewInfo.format				= applyFormat;
	viewInfo.components			= components;
	viewInfo.subresourceRange	= subresourceRange;

	VKASSERT3(vkCreateImageView(*device, &viewInfo, callbacks, &imageView), "");

	return ZImageView::create(imageView, device, callbacks, viewInfo, image);
}

std::pair<VkDeviceSize, VkDeviceSize>
imageGetMipLevelsOffsetAndSize	(ZImage image, uint32_t baseLevel, uint32_t levelCount)
{
	ASSERTMSG(image.has_handle(), "Image must have a handle");
	add_cref<VkImageCreateInfo> ici = imageGetCreateInfo(image);
	ASSERTMSG(baseLevel < ici.mipLevels, "Mip level must not exceed available image levels");
	if (levelCount > (ici.mipLevels - baseLevel)) levelCount = ici.mipLevels - baseLevel;
	return computeMipLevelsOffsetAndSize(ici.format, ici.extent.width, ici.extent.height, baseLevel, levelCount);
}

VkDeviceSize imageCalcMipLevelsSize (ZImage image, uint32_t baseMipLevel, uint32_t mipLevelCount)
{
	ASSERTMSG(image.has_handle(), "Image must have a handle");
	add_cref<VkImageCreateInfo> ici = imageGetCreateInfo(image);
	ASSERTMSG(baseMipLevel < ici.mipLevels, "Mip level must not exceed available image levels");
	if (mipLevelCount > (ici.mipLevels - baseMipLevel)) mipLevelCount = ici.mipLevels - baseMipLevel;
	return imageGetMipLevelsOffsetAndSize(image, baseMipLevel, mipLevelCount).second;
}

add_cref<VkImageCreateInfo> imageGetCreateInfo (add_cref<ZImage> image)
{
	return image.getParamRef<VkImageCreateInfo>();
}

add_cref<VkExtent3D> imageGetExtent (add_cref<ZImage> image)
{
	return image.getParamRef<VkImageCreateInfo>().extent;
}

VkFormat imageGetFormat	(ZImage image)
{
	return image.getParamRef<VkImageCreateInfo>().format;
}

uint32_t imageGetPixelWidth (ZImage image)
{
	return formatGetInfo(imageGetCreateInfo(image).format).pixelByteSize;
}

ZImage imageViewGetImage (ZImageView view)
{
	return view.getParam<ZImage>();
}

VkImageLayout imageGetLayout (ZImage image)
{
	return imageGetCreateInfo(image).initialLayout;
}

VkImageLayout imageResetLayout (ZImage image, VkImageLayout layout)
{
	VkImageCreateInfo& info = image.getParamRef<VkImageCreateInfo>();
	VkImageLayout oldLayout = info.initialLayout;
	info.initialLayout = layout;
	return oldLayout;
}

VkImageLayout imageResetLayout (ZImageView view, VkImageLayout layout)
{
	return imageResetLayout(view.getParam<ZImage>(), layout);
}

VkImageSubresourceRange	imageMakeSubresourceRange (ZImage image,
												   uint32_t baseMipLevel, uint32_t levelCount,
												   uint32_t baseArrayLayer, uint32_t layerCount)
{
	add_cref<VkImageCreateInfo> ci = imageGetCreateInfo(image);
	ASSERTMSG((baseMipLevel < ci.mipLevels) && ((baseMipLevel + levelCount) <= ci.mipLevels),
			  "baseMipLevel must not exceed of image mip level count");
	ASSERTMSG((baseArrayLayer < ci.arrayLayers) && ((baseArrayLayer + layerCount) <= ci.arrayLayers),
			  "baseArrayLayer must not exceed of image array layer count");

	VkImageSubresourceRange r{};
	r.aspectMask		= formatGetAspectMask(ci.format);
	r.baseMipLevel		= baseMipLevel;
	r.levelCount		= levelCount;
	r.baseArrayLayer	= baseArrayLayer;
	r.layerCount		= layerCount;
	return r;
}

VkImageSubresourceLayers imageMakeSubresourceLayers (ZImage image, uint32_t mipLevel,
													 uint32_t baseArrayLayer, uint32_t layerCount)
{
	add_cref<VkImageCreateInfo> ci = imageGetCreateInfo(image);
	ASSERTION((layerCount != 0u) && (baseArrayLayer < ci.arrayLayers) && ((baseArrayLayer + layerCount) <= ci.arrayLayers));
	VkImageSubresourceLayers res{};
	res.aspectMask		= formatGetAspectMask(ci.format);;
	res.mipLevel		= mipLevel;
	res.baseArrayLayer	= baseArrayLayer;
	res.layerCount		= layerCount;
	return res;
}

void imageCopyToBuffer (ZCommandPool commandPool, ZImage image, ZBuffer buffer,
						uint32_t baseLevel, uint32_t levels,
						uint32_t baseLayer, uint32_t layers)
{
	auto shotCommand = createOneShotCommandBuffer(commandPool);
	imageCopyToBuffer(shotCommand->commandBuffer, image, buffer,
					  VK_ACCESS_NONE, VK_ACCESS_NONE,
					  VK_ACCESS_NONE, VK_ACCESS_NONE,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					  baseLevel, levels,
					  baseLayer, layers);
}

void imageCopyToBuffer (ZCommandBuffer commandBuffer,
						ZImage image, ZBuffer buffer,
						VkAccessFlags srcImageAccess, VkAccessFlags dstImageAccess,
						VkAccessFlags srcBufferAccess, VkAccessFlags dstBufferAccess,
						VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
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

	ZImageMemoryBarrier			preImageBarrier		= makeImageMemoryBarrier(
				image, srcImageAccess, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	ZImageMemoryBarrier			postImageBarrier	= makeImageMemoryBarrier(
				image, VK_ACCESS_TRANSFER_READ_BIT, dstImageAccess, imageSavedLayout);
	ZBufferMemoryBarrier		preBufferBarrier	= makeBufferMemoryBarrier(
				buffer, srcBufferAccess, VK_ACCESS_TRANSFER_WRITE_BIT);
	ZBufferMemoryBarrier		postBufferBarrier	= makeBufferMemoryBarrier(
				buffer,	VK_ACCESS_TRANSFER_WRITE_BIT, dstBufferAccess);

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
		}
	}

	commandBufferPipelineBarriers(commandBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, preBufferBarrier, preImageBarrier);
	vkCmdCopyImageToBuffer(*commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, uint32_t(regions.size()), regions.data());
	commandBufferPipelineBarriers(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, postImageBarrier, postBufferBarrier);
}

void imageCopyToImage (ZCommandBuffer cmdBuffer, ZImage srcImage, ZImage dstImage,
					   uint32_t srcArrayLayer, uint32_t arrayLayers, uint32_t dstArrayLayer,
					   uint32_t srcMipLevel, uint32_t mipLevels, uint32_t dstMipLevel,
					   VkAccessFlags inSrcAccess, VkAccessFlags outSrcAccess,
					   VkAccessFlags inDstAccess, VkAccessFlags outDstAccess,
					   VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
					   VkImageLayout finalSrcLayout, VkImageLayout finalDstLayout)
{
	const VkImageCreateInfo&	srcInfo	= imageGetCreateInfo(srcImage);
	const VkImageCreateInfo&	dstInfo	= imageGetCreateInfo(dstImage);

	ASSERTMSG(srcInfo.samples == dstInfo.samples, "The sample count of srcImage and dstImage must match");

	const bool okSrcLayers = (srcArrayLayer < srcInfo.arrayLayers) && ((srcArrayLayer + arrayLayers) <= srcInfo.arrayLayers);
	const bool okDstLayers = (dstArrayLayer < dstInfo.arrayLayers) && ((dstArrayLayer + arrayLayers) <= dstInfo.arrayLayers);
	const bool okSrcLevels = (srcMipLevel < srcInfo.mipLevels) && ((srcMipLevel + mipLevels) <= srcInfo.mipLevels);
	const bool okDstLevels = (dstMipLevel < dstInfo.mipLevels) && ((dstMipLevel + mipLevels) <= dstInfo.mipLevels);

	// cza sprawdzić, czy docelowy level pomieści źródłowy

	auto [srcLevelWidth, srcLevelHeight] = computeMipLevelWidthAndHeight(srcInfo.extent.width, srcInfo.extent.height, srcMipLevel);
	const auto [dstLevelWidth, dstLevelHeight] = computeMipLevelWidthAndHeight(dstInfo.extent.width, dstInfo.extent.height, dstMipLevel);
	const bool okDimmensions = (dstLevelWidth <= srcLevelWidth) && (dstLevelHeight <= srcLevelHeight);

	ASSERTMSG(okSrcLayers && okDstLayers && okSrcLevels && okDstLevels && okDimmensions,
			"Destination mip levels must accomodate at least source levels count or size");

	if (finalSrcLayout == VK_IMAGE_LAYOUT_UNDEFINED) finalSrcLayout = srcInfo.initialLayout;
	if (finalDstLayout == VK_IMAGE_LAYOUT_UNDEFINED) finalDstLayout = dstInfo.initialLayout;

	ZImageMemoryBarrier	preSrcBarrier(srcImage, inSrcAccess, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			imageMakeSubresourceRange(srcImage, srcMipLevel, mipLevels, srcArrayLayer, arrayLayers));
	ZImageMemoryBarrier	preDstBarrier(dstImage, inDstAccess, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageMakeSubresourceRange(dstImage, dstMipLevel, mipLevels, dstArrayLayer, arrayLayers));
	ZImageMemoryBarrier	postSrcBarrier(srcImage, VK_ACCESS_MEMORY_READ_BIT, outSrcAccess, finalSrcLayout,
			imageMakeSubresourceRange(srcImage, srcMipLevel, mipLevels, srcArrayLayer, arrayLayers));
	ZImageMemoryBarrier	postDstBarrier(dstImage, VK_ACCESS_MEMORY_WRITE_BIT, outDstAccess, finalDstLayout,
			imageMakeSubresourceRange(dstImage, dstMipLevel, mipLevels, dstArrayLayer, arrayLayers));

	std::vector<VkImageCopy> regions;


	for (uint32_t layer = 0; layer < arrayLayers; ++layer)
	{
		for (uint32_t level = 0; level < mipLevels; ++level)
		{
			VkImageCopy region{};
			region.srcSubresource	= imageMakeSubresourceLayers(srcImage, (srcMipLevel + level),
																(srcArrayLayer + layer), arrayLayers);
			region.srcOffset		= makeOffset3D();
			region.dstSubresource	= imageMakeSubresourceLayers(dstImage, (dstMipLevel + level),
																(dstArrayLayer + layer), arrayLayers);
			region.dstOffset		= makeOffset3D();
			std::tie(srcLevelWidth, srcLevelHeight) = computeMipLevelWidthAndHeight(
																srcInfo.extent.width, srcInfo.extent.height,
																(srcMipLevel + level));
			region.extent			= makeExtent3D(srcLevelWidth, srcLevelHeight, 1u);
			regions.emplace_back(region);
		}
	}

	commandBufferPipelineBarriers(cmdBuffer, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, preSrcBarrier, preDstBarrier);
	vkCmdCopyImage(*cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							   *dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   data_count(regions), data_or_null(regions));
	commandBufferPipelineBarriers(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, postSrcBarrier, postDstBarrier);
}

} // namespace vtf
