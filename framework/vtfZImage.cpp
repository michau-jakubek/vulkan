#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfZImage.hpp"
#include "vtfFilesystem.hpp"

#include <thread>

#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpragmas\"")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic ignored \"-Wimplicit-int-conversion\"")
#include "stb_image.h"
_Pragma("GCC diagnostic pop")

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

ZSampler createSampler (ZDevice device, const VkSamplerCreateInfo& samplerCreateInfo)
{
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();
	ZSampler sampler(device, callbacks, samplerCreateInfo);
	VKASSERT(vkCreateSampler(*device, &samplerCreateInfo, callbacks, sampler.setter()), "Failed to create sampler");
	return sampler;
}

ZImage createImage (ZDevice device, VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage,
					uint32_t mipLevels, uint32_t layers, VkSampleCountFlagBits samples, VkMemoryPropertyFlags properties)
{
	const auto availableLevels	= computeMipLevelCount(width, height);
	const auto effectiveUsage	= VkImageUsageFlags(usage) | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const auto effectiveLevels	= (INVALID_UINT32 == mipLevels) ? availableLevels : mipLevels ? std::min(mipLevels, availableLevels) : 1;
	const auto callbacks		= device.getParam<VkAllocationCallbacksPtr>();

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
	imageInfo.usage			= effectiveUsage;
	imageInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VkImage	image = VK_NULL_HANDLE;
	VKASSERT2(vkCreateImage(*device, &imageInfo, callbacks, &image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, image, &memRequirements);

	ZDeviceMemory	imageMemory = createMemory(device, memRequirements, properties);

	vkBindImageMemory(*device, image, *imageMemory, 0);

	return ZImage::create(image, device, callbacks, imageInfo, imageMemory, memRequirements.size);
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

bool readImageFileMetadata (const std::string& fileName, add_ref<VkFormat> format,
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
		case 1:	format = VK_FORMAT_R32_SFLOAT;			break;
		case 2: format = VK_FORMAT_R32G32_SFLOAT;		break;
		case 3: format = VK_FORMAT_R32G32B32_SFLOAT;	break;
		case 4: format = VK_FORMAT_R32G32B32A32_SFLOAT;	break;
		default:	result = false;
		}
	}
	return result;
}

template<class Comp, size_t N>
std::vector<VecX<Comp,4>> completeVector (const std::vector<VecX<Comp,N>>& src, std::initializer_list<Comp> missings)
{
	ASSERTION(missings.size() >= (4 - N));
	const auto					size = src.size();
	std::vector<VecX<Comp,4>>	res(size);
	VecX<Comp,4>				v;

	for (std::remove_const_t<decltype(size)> i = 0; i < size; ++i)
	{
		 v.assign(src[i]);

		 auto mb = missings.begin();
		 for (size_t j = N; j < 4; ++j)
			 v[j] = *mb++;

		 res[i] = v;
	}
	return res;
}

std::vector<Vec4> loadImageFileContent (const std::string& fileName)
{
	uint32_t	width	= 0;
	uint32_t	height	= 0;
	VkFormat	format	= VK_FORMAT_UNDEFINED;
	ASSERTION(readImageFileMetadata(fileName, format, width, height));

	static_assert(std::is_same<std::remove_pointer_t<routine_res_t<decltype(stbi_loadf)>>, float>::value, "");
	routine_res_t<decltype(stbi_loadf)>								data		{};
	std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 1>>	sinkWidth	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 2>>	sinkHeight	= 0;
	std::remove_pointer_t<routine_arg_t<decltype(stbi_loadf), 3>>	sinkNcomp	= 0;

	data = stbi_loadf(fileName.c_str(), &sinkWidth, &sinkHeight, &sinkNcomp, 4);
	std::unique_ptr<float, void(*)(float*)> k(data, [](auto ptr){ stbi_image_free(ptr); });

	ASSERTION(data);
	ASSERTION(make_signed(width) == sinkWidth);
	ASSERTION(make_signed(height) == sinkHeight);

	const uint32_t		pixelCount	= (width * height);

	std::vector<Vec4>	result;
	std::vector<Vec3>	v_3;
	std::vector<Vec2>	v_2;
	std::vector<Vec1>	v_1;

	auto bound_4 = makeStdBeginEnd<Vec4>(data, pixelCount);
	auto bound_3 = makeStdBeginEnd<Vec3>(data, pixelCount);
	auto bound_2 = makeStdBeginEnd<Vec2>(data, pixelCount);
	auto bound_1 = makeStdBeginEnd<Vec1>(data, pixelCount);

	sinkNcomp = 4;

	switch (sinkNcomp)
	{
	case 4:
		result.resize(pixelCount);
		std::copy(bound_4.first, bound_4.second, result.begin());
		break;
	case 3:
		v_3.resize(pixelCount);
		std::copy(bound_3.first, bound_3.second, v_3.begin());
		result = completeVector(v_3, { 1.0f });
		break;
	case 2:
		v_2.resize(pixelCount);
		std::copy(bound_2.first, bound_2.second, v_2.begin());
		result = completeVector(v_2, { 0.0f, 1.0f });
		break;
	case 1:
		v_1.resize(pixelCount);
		std::copy(bound_1.first, bound_1.second, v_1.begin());
		result = completeVector(v_1, { 0.0f, 0.0f, 1.0f });
		break;
	}

	return result;
}

ZBuffer	createBufferAndLoadFromImageFile	(ZDevice device, const std::string& fileName, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const std::vector<Vec4>	data	= loadImageFileContent(fileName);
	ZBuffer					buffer	= createBuffer	(device, VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(data.size()), usage, properties);
	writeBuffer(buffer, data);
	return buffer;
}

ImageLoadHelper::ImageLoadHelper (ZImage image, const std::string& imageFileName, bool loadBufferNow)
	: m_srcImage			(image)
	, m_dstImage			(image)
	, m_fileName			(imageFileName)
	, m_buffer				{}
	, m_fileImageWidth		(0)
	, m_fileImageHeight		(0)
	, m_fileImageFormat		(VK_FORMAT_UNDEFINED)
	, m_finalLayout			(VK_IMAGE_LAYOUT_UNDEFINED)
	, m_finalAccess			(0)
	, m_finalPipelineStage	(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
	const bool ok = readImageFileMetadata(m_fileName, m_fileImageFormat, m_fileImageWidth, m_fileImageHeight);
	ASSERTION(ok);
	if (loadBufferNow)
	{
		const auto usage = ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		const auto props = ZMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		ZDevice device = m_srcImage.getParam<ZDevice>();
		m_buffer = createBufferAndLoadFromImageFile(device, m_fileName, usage, props);
	}
	m_fileImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
}

void loadImageFromFile	(ZImage image, const std::string& imageFileName, ZCommandPool commandPool, VkImageLayout finalLayout)
{
	ImageLoadHelper	helper(image, imageFileName, true);
	{
		auto oneShotCommand	= createOneShotCommandBuffer(commandPool);
		loadImageFromFile(helper, oneShotCommand->commandBuffer, finalLayout,
						  0, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	}
}

void loadImageFromFile	(ImageLoadHelper& helper, ZCommandBuffer commandBuffer, VkImageLayout finalLayout,
						 VkAccessFlags srcAccess, VkAccessFlags dstAccess,
						 VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
	ZDevice						device	= commandBuffer.getParam<ZDevice>();
	add_cref<VkImageCreateInfo>	ci		= helper.m_dstImage.getParamRef<VkImageCreateInfo>();
	const ZImageUsageFlags		usage	= ZImageUsageFlags::fromFlags(ci.usage);
	const uint32_t				dWidth	= ci.extent.width;
	const uint32_t				dHeight	= ci.extent.height;
	const VkImageAspectFlags	aspect	= VK_IMAGE_ASPECT_COLOR_BIT;

	ASSERTION(ci.extent.depth == 1u);
	ASSERTION(ci.format == VK_FORMAT_R32G32B32A32_SFLOAT);
	ASSERTION((ci.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u);

	const bool needNew = (dWidth != helper.m_fileImageWidth || dHeight != helper.m_fileImageHeight);

	if (needNew)
	{
		helper.m_srcImage = createImage(device, VK_FORMAT_R32G32B32A32_SFLOAT,
										helper.m_fileImageWidth, helper.m_fileImageHeight,
										(usage + VK_IMAGE_USAGE_TRANSFER_SRC_BIT + VK_IMAGE_USAGE_TRANSFER_DST_BIT),
										1u, 1u, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	if ( ! helper.m_buffer.has_value())
	{
		helper.m_buffer = createBufferAndLoadFromImageFile(
							device, helper.m_fileName,
							ZBufferUsageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
							ZMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	}

	VkBufferImageCopy region{};
	region.bufferOffset					= 0;
	region.bufferRowLength				= 0;
	region.bufferImageHeight			= 0;
	region.imageSubresource.aspectMask	= aspect;
	region.imageSubresource.mipLevel	= 0;
	region.imageSubresource.layerCount	= 1;
	region.imageOffset					= {0, 0, 0};
	region.imageExtent					= { helper.m_fileImageWidth, helper.m_fileImageHeight, 1u};

	transitionImage(commandBuffer, helper.m_srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					srcAccess, VK_ACCESS_TRANSFER_WRITE_BIT,
					srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT);

	vkCmdCopyBufferToImage(*commandBuffer, **helper.m_buffer, *helper.m_srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

	if (needNew)
	{
		VkFormatProperties	props;
		vkGetPhysicalDeviceFormatProperties(*device.getParam<ZPhysicalDevice>(), VK_FORMAT_R32G32B32A32_SFLOAT, &props);
		const VkFormatFeatureFlags flags = VK_IMAGE_TILING_LINEAR == helper.m_srcImage.getParamRef<VkImageCreateInfo>().tiling
											? props.linearTilingFeatures : props.optimalTilingFeatures;
		ASSERTION(flags & (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT));

		VkImageSubresourceLayers	layers;
		layers.aspectMask		= aspect;
		layers.mipLevel			= 0;
		layers.baseArrayLayer	= 0;
		layers.layerCount		= 1;

		VkImageBlit	blit;
		blit.srcSubresource		= layers;
		blit.dstSubresource		= layers;
		blit.srcOffsets[0]		= {                                    0,                                     0, 0 };
		blit.srcOffsets[1]		= { make_signed(helper.m_fileImageWidth), make_signed(helper.m_fileImageHeight), 1 };
		blit.dstOffsets[0]		= {                                    0,                                     0, 0 };
		blit.dstOffsets[1]		= {                  make_signed(dWidth),                  make_signed(dHeight), 1 };

		VkFilter	filter = (flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

		transitionImage(commandBuffer, helper.m_srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		transitionImage(commandBuffer, helper.m_dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						srcAccess, VK_ACCESS_TRANSFER_WRITE_BIT,
						srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdBlitImage(*commandBuffer,
					   *helper.m_srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   *helper.m_dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &blit, filter);
	}

	transitionImage(commandBuffer, helper.m_dstImage, finalLayout,
					VK_ACCESS_TRANSFER_WRITE_BIT, dstAccess,
					VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage);
}

ZImage createImageFromFileMetadata	(ZDevice device,
									 const std::string& fileName, ZImageUsageFlags usage,
									 const AnySize& desiredWidth, const AnySize& desiredHeight)
{
	UNREF(desiredWidth);
	UNREF(desiredHeight);
	uint32_t	imageWidth	= 0;
	uint32_t	imageHeight	= 0;
	VkFormat	imageFormat	= VK_FORMAT_UNDEFINED;
	ASSERTION(readImageFileMetadata(fileName, imageFormat, imageWidth, imageHeight));
	ZImage image = createImage(device, VK_FORMAT_R32G32B32A32_SFLOAT,
							   desiredWidth.valid() ? desiredWidth.get(imageWidth, imageHeight, 1u) : imageWidth,
							   desiredHeight.valid() ? desiredHeight.get(imageWidth, imageHeight, 1u) : imageHeight,
							   usage, 1u, 1u, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	return image;
}

ZImage createImageAndLoadFromFile	(ZCommandPool commandPool, const std::string& fileName, ZImageUsageFlags usage,
									 const AnySize& desiredWidth, const AnySize& desiredHeight, VkImageLayout finalLayout)
{
	ZImage	image	= createImageFromFileMetadata(commandPool.getParam<ZDevice>(), fileName, usage, desiredWidth, desiredHeight);
	loadImageFromFile(image, fileName, commandPool, finalLayout);
	return image;
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
										 ZMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

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
			writeBufferData(buffer, reinterpret_cast<add_ptr<uint8_t>>(data), bufferSize);
		}
		stbi_image_free(data);
		buffers.emplace_back(buffer);
	}

	ZImage image = createImage(device, dataFormat, width, height, usage, 1u, 6u, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

		transitionImage(oneShotCommand->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						0, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		for (uint32_t layer = 0; layer < 6; ++layer)
		{
			region.imageSubresource.baseArrayLayer = layer;
			vkCmdCopyBufferToImage(*oneShotCommand->commandBuffer, *buffers[layer], *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
		}

		transitionImage(oneShotCommand->commandBuffer, image, finalLayout,
						VK_ACCESS_TRANSFER_WRITE_BIT, finalAccess,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
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

ZImageView createImageView (ZImage image, VkImageAspectFlags aspect, VkFormat chgfmt, VkImageViewType viewType,
							uint32_t baseLevel, uint32_t levels, uint32_t baseLayer, uint32_t layers)
{
	VkImageView					imageView	= VK_NULL_HANDLE;
	add_cref<VkImageCreateInfo>	imageInfo	= image.getParam<VkImageCreateInfo>();
	const bool					cubeMap		= (imageInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0u;
	ZDevice						device		= image.getParam<ZDevice>();
	VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();

	const uint32_t			finalBaseLevel	= (INVALID_UINT32 == baseLevel) ? 0 : baseLevel;
	const uint32_t			finalBaseLayer	= (INVALID_UINT32 == baseLayer) ? 0 : baseLayer;
	const uint32_t			finalLevelCount	= (INVALID_UINT32 == levels)
												? imageInfo.mipLevels
												: std::min((imageInfo.mipLevels - baseLevel), levels);
	const uint32_t			finalLayerCount	= (INVALID_UINT32 == layers)
												? imageInfo.arrayLayers
												: std::min((imageInfo.arrayLayers - baseLayer), layers);
	const VkImageViewType	finalViewType	= (VK_IMAGE_VIEW_TYPE_MAX_ENUM != viewType)
												? viewType
												: cubeMap
												  ? VK_IMAGE_VIEW_TYPE_CUBE
												  : imageTypeToViewType(imageInfo.imageType);
	ASSERTION(finalLevelCount >= 1u);
	ASSERTION(finalLayerCount >= (cubeMap ? 6u : 1u));

	VkComponentMapping components{};
	components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask		= aspect;
	subresourceRange.baseMipLevel	= finalBaseLevel;
	subresourceRange.levelCount		= finalLevelCount;
	subresourceRange.baseArrayLayer	= finalBaseLayer;
	subresourceRange.layerCount		= finalLayerCount;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image				= *image;
	viewInfo.viewType			= finalViewType;
	viewInfo.format				= chgfmt == VK_FORMAT_UNDEFINED ? imageInfo.format : chgfmt;
	viewInfo.components			= components;
	viewInfo.subresourceRange	= subresourceRange;

	VKASSERT2(vkCreateImageView(*device, &viewInfo, callbacks, &imageView));

	return ZImageView::create(imageView, device, callbacks, viewInfo, image);
}

VkImageLayout changeImageLayout (ZImage image, VkImageLayout layout)
{
	VkImageCreateInfo& info = image.getParamRef<VkImageCreateInfo>();
	VkImageLayout oldLayout = info.initialLayout;
	info.initialLayout = layout;
	return oldLayout;
}

VkImageLayout changeImageLayout (ZImageView view, VkImageLayout layout)
{
	return changeImageLayout(view.getParam<ZImage>(), layout);
}

void	transitionImage (ZCommandBuffer cmd, VkImage image, VkFormat format,
						 VkImageLayout			initialLayout,	VkImageLayout			targetLayout,
						 VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
						 VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage)
{
	UNREF(format);

	VkImageMemoryBarrier	barrier			{};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.oldLayout			= initialLayout;
	barrier.newLayout			= targetLayout;
	barrier.srcAccessMask		= sourceAccess;
	barrier.dstAccessMask		= destinationAccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= image;
	barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT; // TODO: It should depend on image format
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= 1;
	barrier.subresourceRange.baseArrayLayer	= 0;
	barrier.subresourceRange.layerCount		= 1;

	vkCmdPipelineBarrier(*cmd, sourceStage, destinationStage, VK_DEPENDENCY_BY_REGION_BIT,
						 0, (const VkMemoryBarrier*)nullptr,
						 0, (const VkBufferMemoryBarrier*)nullptr,
						 1, &barrier);
}

void transitionImage (ZCommandBuffer cmd, ZImage image, VkImageLayout targetLayout,
					  VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
					  VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage)
{
	const VkImageMemoryBarrier	barrier		= makeImageMemoryBarrier(image, sourceAccess, destinationAccess, targetLayout);
	commandBufferPipelineBarrier(cmd, barrier, sourceStage, destinationStage);
	image.getParamRef<VkImageCreateInfo>().initialLayout	= targetLayout;
}

VkImageMemoryBarrier makeImageMemoryBarrier	(ZImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout targetLayout)
{
	add_cref<VkImageCreateInfo>	createInfo	= image.getParamRef<VkImageCreateInfo>();

	VkImageMemoryBarrier	barrier{};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.oldLayout			= createInfo.initialLayout;
	barrier.newLayout			= (targetLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? createInfo.initialLayout : targetLayout;
	barrier.srcAccessMask		= srcAccess;
	barrier.dstAccessMask		= dstAccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= *image;
	barrier.subresourceRange	= makeImageSubresourceRange(image);

	return barrier;
}

VkImageSubresourceRange	makeImageSubresourceRange	(ZImage image)
{
	add_cref<VkImageCreateInfo>	createInfo	= image.getParamRef<VkImageCreateInfo>();

	VkImageSubresourceRange		range{};
	range.aspectMask			= VK_IMAGE_ASPECT_COLOR_BIT; // TODO: should depend on image format
	range.baseMipLevel			= 0u;
	range.levelCount			= createInfo.mipLevels;
	range.baseArrayLayer		= 0u;
	range.layerCount			= createInfo.arrayLayers;

	return range;
}

void copyImageToBuffer (ZCommandPool commandPool, ZImageView view, ZBuffer buffer, bool zeroBuffer)
{
	add_cref<VkImageViewCreateInfo>	info	= view.getParamRef<VkImageViewCreateInfo>();
	ZImage							image	= view.getParam<ZImage>();
	copyImageToBuffer(commandPool, image, buffer,
					  info.subresourceRange.baseMipLevel, info.subresourceRange.levelCount,
					  info.subresourceRange.baseArrayLayer, info.subresourceRange.layerCount, zeroBuffer);
}

void copyImageToBuffer (ZCommandPool commandPool, ZImage image, ZBuffer buffer,
						uint32_t baseLevel, uint32_t levels,
						uint32_t baseLayer, uint32_t layers, bool zeroBuffer)
{
	auto shotCommand = createOneShotCommandBuffer(commandPool);
	transitionImage(shotCommand->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					0/*VK_ACCESS_NONE*/, VK_ACCESS_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	copyImageToBuffer(shotCommand->commandBuffer, image, buffer, baseLevel, levels, baseLayer, layers, zeroBuffer);
}

void copyImageToBuffer (ZCommandBuffer commandBuffer,
						ZImage image, ZBuffer buffer,
						uint32_t baseLevel, uint32_t levelCount,
						uint32_t baseLayer, uint32_t layerCount, bool zeroBuffer)
{
	const auto					createInfo		= image.getParam<VkImageCreateInfo>();
	const uint32_t				pixelWidth		= make_unsigned(computePixelByteWidth(createInfo.format));
	const VkImageAspectFlags	aspect			= VK_IMAGE_ASPECT_COLOR_BIT; // TODO: depends on format
	const VkDeviceSize			bufferSize		= computeBufferSize(createInfo.format,
																	createInfo.extent.width, createInfo.extent.height,
																	baseLevel, levelCount, layerCount);
	const VkBufferMemoryBarrier	buffBarrier		= makeBufferMemoryBarrier(buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);


	ASSERTION(0 < levelCount && (baseLevel + levelCount) <= createInfo.mipLevels);
	ASSERTION(bufferSize <= buffer.getParam<VkDeviceSize>());

	VkImageLayout			finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	/*
	VkImageSubresourceRange	range{};
	range.aspectMask		= aspect;
	range.baseMipLevel		= baseLevel;
	range.levelCount		= levelCount;
	range.baseArrayLayer	= baseLayer;
	range.layerCount		= layerCount;
	*/

	VkBufferImageCopy		rgn{};
	rgn.bufferOffset		= 0u;
	rgn.bufferRowLength		= 0u; // assume tightly packed
	rgn.bufferImageHeight	= 0u; // assume tightly packed
	rgn.imageSubresource	= {};
	rgn.imageOffset			= { 0u, 0u, 0u };
	rgn.imageExtent			= { 0u, 0u, 1u };

	add_ref<VkImageSubresourceLayers> rsc(rgn.imageSubresource);
	rsc.aspectMask			= aspect;
	rsc.mipLevel			= baseLevel;
	rsc.baseArrayLayer		= baseLayer;
	rsc.layerCount			= layerCount;

	VkDeviceSize	bufferOffset	= 0;
	std::vector<VkBufferImageCopy>	regions(layerCount * levelCount);

	if (zeroBuffer)
	{
		const auto zeroBarrier = makeBufferMemoryBarrier(buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		vkCmdFillBuffer(*commandBuffer, *buffer, 0u, VK_WHOLE_SIZE, 0u);
		commandBufferPipelineBarrier(commandBuffer, zeroBarrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	}

	for (uint32_t layer = 0; layer < layerCount; ++layer)
	{
		uint32_t		width			= (createInfo.extent.width >> baseLevel);
		uint32_t		height			= (createInfo.extent.height >> baseLevel);

		for (uint32_t mipLevel = 0; mipLevel < levelCount; ++mipLevel)
		{
			ASSERTION(width > 0 && height > 0);

			rgn.bufferOffset		= bufferOffset;
			rgn.bufferRowLength		= width;
			rgn.bufferImageHeight	= height;
			rgn.imageExtent			= { width, height, 1u };
			rsc.mipLevel			= mipLevel + baseLevel;
			rsc.baseArrayLayer		= layer + baseLayer;
			rsc.layerCount			= 1;

			regions[layer * levelCount + mipLevel] = rgn;

			bufferOffset += (width * height * pixelWidth);
			height /= 2;
			width /= 2;
		}
	}
	vkCmdCopyImageToBuffer(*commandBuffer,
						   *image, finalLayout, *buffer,
						   static_cast<uint32_t>(regions.size()), regions.data());

	commandBufferPipelineBarrier(commandBuffer, buffBarrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
}

} // namespace vtf
