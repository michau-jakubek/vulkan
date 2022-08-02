#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfVertexInput.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfDebugMessenger.hpp"

namespace vtf
{

VkClearValue makeClearColor (const Vec4& color)
{
	return { color[0], color[1], color[2], color[3] };
}

std::ostream& operator<<(std::ostream& str, const Version& v)
{
	str << "(" << v.nmajor << ", " << v.nminor << ", " << v.npatch << ")";
	return str;
}

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
	const auto effectiveUsage	= usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const auto effectiveLevels	= (uint32_t(~0) == mipLevels) ? availableLevels : mipLevels ? std::min(mipLevels, availableLevels) : 1;
	const auto callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType		= VK_IMAGE_TYPE_2D;
	imageInfo.extent.width	= width;
	imageInfo.extent.height	= height;
	imageInfo.extent.depth	= 1;
	imageInfo.mipLevels		= effectiveLevels;
	imageInfo.arrayLayers	= layers;
	imageInfo.format		= format;
	imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL,
	imageInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage			= effectiveUsage;
	imageInfo.samples		= samples;
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VkImage	image = VK_NULL_HANDLE;
	VKASSERT2(vkCreateImage(*device, &imageInfo, callbacks, &image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, image, &memRequirements);

	ZDeviceMemory	imageMemory = createMemory(device, memRequirements, properties);

	vkBindImageMemory(*device, image, *imageMemory, 0);

	return ZImage::create(image, device, callbacks, imageInfo, imageMemory, memRequirements.size);
}

ZImageView createImageView (ZImage image, VkImageAspectFlags aspect, VkFormat chgfmt,
							uint32_t baseLevel, uint32_t levels, uint32_t baseLayer, uint32_t layers)
{
	VkImageView					imageView	= VK_NULL_HANDLE;
	const VkImageCreateInfo		imageInfo	= image.getParam<VkImageCreateInfo>();
	ZDevice						device		= image.getParam<ZDevice>();
	VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();

	const uint32_t			levelCount	= std::min((imageInfo.mipLevels - baseLevel), levels);
	const uint32_t			layerCount	= std::min((imageInfo.arrayLayers - baseLayer), layers);

	ASSERTION(levelCount >= 1);
	ASSERTION(layerCount >= 1);

	VkComponentMapping components{};
	components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask		= aspect;
	subresourceRange.baseMipLevel	= baseLevel;
	subresourceRange.levelCount		= levelCount;
	subresourceRange.baseArrayLayer	= baseLayer;
	subresourceRange.layerCount		= layerCount;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType				= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image				= *image;
	viewInfo.viewType			= VK_IMAGE_VIEW_TYPE_2D;
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

ZShaderModule createShaderModule(ZDevice device, const std::string& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize	= code.length();
	createInfo.pCode	= reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule				shaderModule	= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	if (vkCreateShaderModule(*device, &createInfo, callbacks, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return ZShaderModule::create(shaderModule, device, callbacks);
}

ZShaderModule createShaderModule(ZDevice device, const std::vector<unsigned char>& code)
{
	auto readMagicNumber = [](const std::vector<unsigned char>& s) -> uint32_t
	{
		return *(const uint32_t*)(s.data());
	};
	auto changeMagicNumber = [](uint32_t magic, std::vector<unsigned char>& s) -> void
	{
		*(uint32_t*)(s.data()) = magic;
	};

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize	= code.size();
	createInfo.pCode	= reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule				shaderModule	= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	std::vector<unsigned char> code2;
	if (readMagicNumber(code) == 0)
	{
		std::cout << "Magic number was: 0x" << std::hex << readMagicNumber(code) << std::dec << std::endl;
		code2 = code;
		changeMagicNumber(0x07230203, code2);
		createInfo.pCode	= reinterpret_cast<const uint32_t*>(code2.data());
		std::cout << "Magic number is: 0x" << std::hex << readMagicNumber(code2) << std::dec << std::endl;
	}

	VKASSERT2(vkCreateShaderModule(*device, &createInfo, callbacks, &shaderModule));

	return ZShaderModule::create(shaderModule, device, callbacks);
}

ZCommandPool createCommandPool(ZDevice device, uint32_t queueFamilyIndex)
{
	VkQueue devQueue = VK_NULL_HANDLE;
	vkGetDeviceQueue(*device, queueFamilyIndex, 0, &devQueue);
	auto	queue	= ZQueue::create(devQueue, queueFamilyIndex);

	return createCommandPool(device, queue);
}

ZCommandPool createCommandPool(ZDevice device, ZQueue queue, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex	= queue.getParam<uint32_t>();
	poolInfo.flags				= flags;

	VkCommandPool				pool = VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks = device.getParam<VkAllocationCallbacksPtr>();

	if (vkCreateCommandPool(*device, &poolInfo, callbacks, &pool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}

	return ZCommandPool::create(pool, device, callbacks, queue);
}

ZCommandBuffer createCommandBuffer (ZCommandPool commandPool)
{
	VkCommandBuffer	commandBuffer	= VK_NULL_HANDLE;
	auto			device			= commandPool.getParam<ZDevice>();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool			= *commandPool;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount	= 1;

	VKASSERT2(vkAllocateCommandBuffers(*device, &allocInfo, &commandBuffer));

	return ZCommandBuffer::create(commandBuffer, device, commandPool);
}

void OneShotCommandBuffer::submit ()
{
	if (!m_submitted)
	{
		auto queue = m_commandPool.getParam<ZQueue>();

		VkCommandBuffer commands[] = { *m_commandBuffer };
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = commands;

		vkEndCommandBuffer(*m_commandBuffer);
		vkQueueSubmit(*queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(*queue);

		m_submitted = true;
	}
}

OneShotCommandBuffer::OneShotCommandBuffer (ZCommandPool pool)
	: submitted			(m_submitted)
	, commandBuffer		(m_commandBuffer)
	, m_submitted		(false)
	, m_commandPool		(pool)
	, m_commandBuffer	(createCommandBuffer(pool))
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(*commandBuffer, &beginInfo);
}

std::unique_ptr<OneShotCommandBuffer> createOneShotCommandBuffer (ZCommandPool commandPool)
{
	std::unique_ptr<OneShotCommandBuffer> u(new OneShotCommandBuffer(commandPool));
	return u;
}

ZInstance		createInstance (const char*					appName,
								VkAllocationCallbacksPtr	callbacks,
								const strings&				requiredLayers,
								const strings&				requiredExtensions,
								VkDebugUtilsMessengerEXT*	pMessenger,
								void*						pMessengerUserData,
								uint32_t					apiVersion,
								uint32_t					engVersion,
								uint32_t					appVersion)
{
	ZInstance			instance			(getAllocationCallbacks(), requiredLayers, {});
	add_ref<strings>	availableExtensions	= instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>().data;

	strings availableLayers;
	if (requiredLayers.size())
	{
		availableLayers = enumerateInstanceLayers();
		if (!containsAllString(requiredLayers, availableLayers))
			ASSERTMSG(false, "All desired layer(s) must match to available layer(s)!!!");
	}

	availableExtensions	= enumerateInstanceExtensions(requiredLayers);

	ASSERTMSG(containsAllString(requiredExtensions, availableExtensions), "All required extensions must match available extensions");

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
	const std::string debugUtilsExtName(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		// VK_EXT_debug_utils
	const std::string debugReportExtName(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// "VK_EXT_debug_report"
	const bool debugMessengerEnabled = (containsString(debugUtilsExtName, requiredExtensions) || pMessenger != nullptr)
										&& (requiredLayers.size() != 0) && containsString(debugUtilsExtName, availableExtensions);
	if (debugMessengerEnabled)
	{
		getDebugCreateInfo(debugMessengerInfo, nullptr);
	}
	else
	{
		removeStrings({ debugUtilsExtName, debugReportExtName }, availableExtensions);
	}

	VkApplicationInfo appInfo{};
	appInfo.sType						= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName			= appName;		//"Some Application Name";
	appInfo.applicationVersion			= appVersion;	//VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName					= "No Engine";
	appInfo.engineVersion				= engVersion;	//VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion					= apiVersion;	//VK_API_VERSION_1_0;

	std::vector<const char*>			v_layerNames(to_cstrings(requiredLayers));
	std::vector<const char*>			v_extensions(to_cstrings(availableExtensions));

	VkInstanceCreateInfo createInfo{};
	createInfo.sType					= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext					= debugMessengerEnabled ? &debugMessengerInfo : nullptr;
	createInfo.pApplicationInfo			= &appInfo;

	createInfo.enabledExtensionCount	= static_cast<uint32_t>(v_extensions.size());
	createInfo.ppEnabledExtensionNames	= v_extensions.size() ? v_extensions.data() : nullptr;
	createInfo.enabledLayerCount		= static_cast<uint32_t>(v_layerNames.size());
	createInfo.ppEnabledLayerNames		= v_layerNames.size() ? v_layerNames.data() : nullptr;

	VKASSERT(vkCreateInstance(&createInfo, getAllocationCallbacks(), instance.setter()), "Failed to create instance!");

	if (debugMessengerEnabled) createDebugMessenger(instance, callbacks, pMessengerUserData, *pMessenger);

	return instance;
}

ZPhysicalDevice	getPhysicalDeviceByIndex (ZInstance instance, uint32_t physicalDeviceIndex)
{
	std::vector<VkPhysicalDevice> devices;
	const uint32_t		deviceCount = enumeratePhysicalDevices(*instance, devices);
	ASSERTMSG(physicalDeviceIndex < deviceCount, "Failed to find GPUs with Vulkan support!");
	add_cref<strings>	layers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>().data;
	const strings		extensions	= enumerateDeviceExtensions(devices.at(physicalDeviceIndex), layers);
	return ZPhysicalDevice::create(devices.at(physicalDeviceIndex), instance.getParam<VkAllocationCallbacksPtr>(), instance, physicalDeviceIndex, extensions);
}

ZPhysicalDevice selectPhysicalDevice(const uint32_t							proposedDeviceIndex,
									 ZInstance								instance,
									 const strings&							requiredExtensions,
									 std::map<VkQueueFlagBits, uint32_t>&	queuesToIndices,
									 std::optional<ZSurfaceKHR>				surface,
									 add_ptr<uint32_t>						pPresentQueueFamilyIndex)
{
	std::vector<VkPhysicalDevice> devices;
	const uint32_t deviceCount = enumeratePhysicalDevices(*instance, devices);
	ASSERTMSG(deviceCount != 0, "Failed to find GPUs that suppors Vulkant!");

	uint32_t			presentQueueIndex	= INVALID_UINT32;
	uint32_t			physicalDeviceIndex	= INVALID_UINT32;
	add_cref<strings>	layers				= instance.getParamRef<ZDistType<RequiredLayers, strings>>().data;
	ZPhysicalDevice		physicalDevice		(instance.getParam<VkAllocationCallbacksPtr>(), instance, physicalDeviceIndex, {});
	add_ref<strings>	availableExtensions	= physicalDevice.getParamRef<strings>();

	ASSERTION(queuesToIndices.size());
	ASSERTION(!(surface.has_value() ^ (nullptr != pPresentQueueFamilyIndex)));
	auto updateQueues = [&](VkPhysicalDevice dev) -> bool
	{
		for (const auto& queueType2Index : queuesToIndices)
		{
			const uint32_t familyIndex = findQueueFamilyIndex(dev, queueType2Index.first);
			if (familyIndex == INVALID_UINT32) return false;
			queuesToIndices[queueType2Index.first] = familyIndex;
		}

		bool result = true;

		if (surface.has_value())
		{
			VkSurfaceKHR	surfaceHandle = **surface;
			presentQueueIndex = findSurfaceSupportedQueueFamilyIndex(dev, surfaceHandle);
			result = (INVALID_UINT32 != presentQueueIndex) && hasFormatsAndModes(dev, surfaceHandle);
		}

		return result;
	};

	VkPhysicalDevice result = VK_NULL_HANDLE;
	if (INVALID_UINT32 == proposedDeviceIndex)
	{
		for (uint32_t i = 0; i < deviceCount; ++i)
		{
			const VkPhysicalDevice device = devices.at(i);
			availableExtensions = enumerateDeviceExtensions(device, layers);
			if (containsAllString(requiredExtensions, availableExtensions) && updateQueues(device))
			{
				physicalDeviceIndex = i;
				result = device;
				break;
			}
		}
	}
	else if (proposedDeviceIndex < deviceCount)
	{
		VkPhysicalDevice device = devices[proposedDeviceIndex];
		availableExtensions = enumerateDeviceExtensions(device, layers);
		if (containsAllString(requiredExtensions, availableExtensions) && updateQueues(device))
		{
			physicalDeviceIndex = proposedDeviceIndex;
			result = device;
		}
	}

	ASSERTMSG(result != VK_NULL_HANDLE, "Failed to find a suitable GPU with desired queues!");

	if (nullptr != pPresentQueueFamilyIndex)
	{
		*pPresentQueueFamilyIndex = presentQueueIndex;
	}

	physicalDevice.setParam<uint32_t>(physicalDeviceIndex);
	physicalDevice.replace(result);

	return physicalDevice;
}

ZDevice createLogicalDevice	(ZPhysicalDevice							physDevice,
							 const std::map<VkQueueFlagBits, uint32_t>&	queuesToIndices,
							 const uint32_t								presentQueueFamilyIndex)
{
	ASSERTMSG(queuesToIndices.size(), "Queue list must not be empty");

	std::vector<float>					queuePriorities(20, 1.0f);

	auto makeQueueCreateInfo = [&](uint32_t familyIndex) -> VkDeviceQueueCreateInfo
	{
		VkDeviceQueueCreateInfo qci;
		qci.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci.pNext				= nullptr;
		qci.flags				= VkDeviceQueueCreateFlags(0);
		qci.queueFamilyIndex	= familyIndex;
		qci.queueCount			= 1;
		qci.pQueuePriorities	= queuePriorities.data();
		return qci;
	};

	std::vector<VkDeviceQueueCreateInfo>	queueCreateInfos;

	auto distinctPush = [&](uint32_t familyIndex)
	{
		for (const auto& qci : queueCreateInfos)
		{
			if (qci.queueFamilyIndex == familyIndex)
				return;
		}
		queueCreateInfos.push_back(makeQueueCreateInfo(familyIndex));
	};

	for (const auto& queueToIndex : queuesToIndices)
	{
		ASSERTMSG(queueToIndex.second != INVALID_UINT32, "Invalid queue family index");
		distinctPush(queueToIndex.second);
	}
	if (presentQueueFamilyIndex != INVALID_UINT32)
	{
		distinctPush(presentQueueFamilyIndex);
	}

	ZInstance					instance	= physDevice.getParam<ZInstance>();
	VkAllocationCallbacksPtr	callbacks	= instance.getParam<VkAllocationCallbacksPtr>();
	std::vector<const char*>	layers		(to_cstrings(instance.getParamRef<ZDistType<RequiredLayers,strings>>().data));
	std::vector<const char*>	extensions	(to_cstrings(physDevice.getParamRef<strings>()));

	VkPhysicalDeviceFeatures	deviceFeatures{};
	vkGetPhysicalDeviceFeatures(*physDevice, &deviceFeatures);

	VkDeviceCreateInfo createInfo{};
	createInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount		= static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos		= queueCreateInfos.data();

	createInfo.pEnabledFeatures			= &deviceFeatures;

	createInfo.enabledExtensionCount	= static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames	= extensions.size() ? extensions.data() : nullptr;
	createInfo.enabledLayerCount		= static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames		= layers.size() ? layers.data() : nullptr;

	ZDevice logicalDevice(callbacks, physDevice, {});
	VKASSERT2(vkCreateDevice(*physDevice, &createInfo, callbacks, logicalDevice.setter()));

	add_ref<ZQueueInfos> queueInfos = logicalDevice.getParamRef<ZQueueInfos>();
	for (const auto& queueToIndex : queuesToIndices)
	{
		VkQueue			handle		= VK_NULL_HANDLE;
		const uint32_t	familyIndex	= queueToIndex.second;
		vkGetDeviceQueue(*logicalDevice, familyIndex, 0, &handle);
		ZQueue queue = ZQueue::create(handle, familyIndex);

		if (queueToIndex.first & VK_QUEUE_GRAPHICS_BIT)
		{
			queueInfos[ZQueueType::Graphics] = queue;
		}
		else if (queueToIndex.first & VK_QUEUE_COMPUTE_BIT)
		{
			queueInfos[ZQueueType::Compute] = queue;
		}
		else if (queueToIndex.first & VK_QUEUE_TRANSFER_BIT)
		{
			queueInfos[ZQueueType::Transfer] = queue;
		}
		else
		{
			ASSERTMSG(0, "Unknown queue type");
		}
	}
	if (presentQueueFamilyIndex != INVALID_UINT32)
	{
		VkQueue handle	= VK_NULL_HANDLE;
		vkGetDeviceQueue(*logicalDevice, presentQueueFamilyIndex, 0, &handle);
		queueInfos[ZQueueType::Present] = ZQueue::create(handle, presentQueueFamilyIndex);
	}

	return logicalDevice;
}

ZFence createFence (ZDevice device, bool signaled)
{
	VkFence handle = VK_NULL_HANDLE;
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();

	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	VKASSERT2(vkCreateFence(*device, &fenceInfo, callbacks, &handle));
	return ZFence::create(handle, device, callbacks);
}

void waitForFence (ZFence fence, uint64_t timeout)
{
	VkDevice device = *fence.getParam<ZDevice>();
	VKASSERT2(vkWaitForFences(device, 1, fence.ptr(), VK_TRUE, timeout));
}

void waitForFences (std::initializer_list<ZFence> fences, uint64_t timeout)
{
	if (auto count = static_cast<uint32_t>(fences.size()); count)
	{
		auto b = fences.begin();
		const ZDevice device = b->getParam<ZDevice>();
		std::vector<VkFence> handles(count);
		for (auto i = b; i != fences.end(); ++i)
		{
			ASSERTION(device == i->getParam<ZDevice>());
			handles[std::distance(b, i)] = **i;
		}
		VKASSERT2(vkWaitForFences(*device, count, handles.data(), VK_TRUE, timeout));
	}
}

void waitForFences (std::vector<ZFence> fences, uint64_t timeout)
{
	if (auto count = static_cast<uint32_t>(fences.size()); count)
	{
		auto b = fences.begin();
		const ZDevice device = b->getParam<ZDevice>();
		std::vector<VkFence> handles(count);
		for (auto i = b; i != fences.end(); ++i)
		{
			ASSERTION(device == i->getParam<ZDevice>());
			handles[std::distance(b, i)] = **i;
		}
		VKASSERT2(vkWaitForFences(*device, count, handles.data(), VK_TRUE, timeout));
	}
}

void resetFence (ZFence fence)
{
	VkDevice device = *fence.getParam<ZDevice>();
	VKASSERT2(vkResetFences(device, 1, fence.ptr()));
}

void resetFences (std::initializer_list<ZFence> fences)
{
	if (auto count = static_cast<uint32_t>(fences.size()); count)
	{
		auto b = fences.begin();
		const ZDevice device = b->getParam<ZDevice>();
		std::vector<VkFence> handles(count);
		for (auto i = b; i != fences.end(); ++i)
		{
			ASSERTION(device == i->getParam<ZDevice>());
			handles[std::distance(b, i)] = **i;
		}
		VKASSERT2(vkResetFences(*device, count, handles.data()));
	}
}

void resetFences (std::vector<ZFence> fences)
{
	if (auto count = static_cast<uint32_t>(fences.size()); count)
	{
		auto b = fences.begin();
		const ZDevice device = b->getParam<ZDevice>();
		std::vector<VkFence> handles(count);
		for (auto i = b; i != fences.end(); ++i)
		{
			ASSERTION(device == i->getParam<ZDevice>());
			handles[std::distance(b, i)] = **i;
		}
		VKASSERT2(vkResetFences(*device, count, handles.data()));
	}
}

ZSemaphore createSemaphore (ZDevice device, bool signaled)
{
	VkSemaphore handle = VK_NULL_HANDLE;
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();

	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semInfo.pNext = nullptr;
	semInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	VKASSERT2(vkCreateSemaphore(*device, &semInfo, callbacks, &handle));

	return ZSemaphore::create(handle, device, callbacks);
}

void commandBufferBindPipeline (ZCommandBuffer cmd, ZPipeline pipeline)
{
	auto bindingPoint		= pipeline.getParam<VkPipelineBindPoint>();
	auto pipelineLayout		= pipeline.getParam<ZPipelineLayout>();
	auto& descriptorLayouts	= pipelineLayout.getParamRef<std::vector<ZDescriptorSetLayout>>();

	std::vector<VkDescriptorSet> sets;
	std::transform(descriptorLayouts.begin(), descriptorLayouts.end(), std::back_inserter(sets),
				   [](const ZDescriptorSetLayout& dsl) { return **dsl.getParam<std::optional<ZDescriptorSet>>(); });

	vkCmdBindPipeline(*cmd, bindingPoint, *pipeline);

	if (sets.size())
	{
		vkCmdBindDescriptorSets(*cmd,
								bindingPoint,
								*pipelineLayout,
								0,				//firstSet
								static_cast<uint32_t>(sets.size()),
								sets.data(),
								0,				//dynamicOffsetCount
								nullptr);		//pDynamicOffsets
	}
}

void commandBufferBindVertexBuffers (ZCommandBuffer cmd, const VertexInput& input, std::initializer_list<ZBuffer> externalBuffers)
{
	auto vertexBuffers	= input.getVertexBuffers(externalBuffers);
	auto vertexOffsets	= input.getVertexOffsets();
	vkCmdBindVertexBuffers(*cmd, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), vertexOffsets.data());
}

void commandBufferBindIndexBuffer (ZCommandBuffer cmd, ZBuffer buffer, VkIndexType indexType)
{
	vkCmdBindIndexBuffer(*cmd, *buffer, 0, indexType);
}

void commandBufferPushConstants(ZCommandBuffer cmd , ZPipelineLayout layout, VkShaderStageFlags flags, uint32_t offset, uint32_t size, const void* p)
{
	vkCmdPushConstants(*cmd, *layout, flags, offset, size, p);
}

void commandBufferEnd (ZCommandBuffer commandBuffer)
{
	VKASSERT2(vkEndCommandBuffer(*commandBuffer));
}

void commandBufferBegin (ZCommandBuffer commandBuffer)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext				= nullptr;
	beginInfo.flags				= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	beginInfo.pInheritanceInfo	= nullptr;

	VKASSERT2(vkBeginCommandBuffer(*commandBuffer, &beginInfo));
}

void commandBufferSubmitAndWait (ZCommandBuffer commandBuffer)
{
	commandBufferSubmitAndWait( { commandBuffer } );
}

void commandBufferSubmitAndWait (std::initializer_list<ZCommandBuffer> commandBuffers)
{
	const uint32_t commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	ASSERTION(commandBufferCount);

	auto	ii		= commandBuffers.begin();
	ZDevice	device	= ii->getParam<ZDevice>();

	std::vector<VkSubmitInfo>	submits(commandBufferCount);
	std::vector<VkFence>		fences(commandBufferCount);
	std::vector<ZFence>			zfences;

	for (auto i = ii; i != commandBuffers.end(); ++i)
	{
		ASSERTION(	device== i->getParam<ZDevice>()	);
		auto at = std::distance(ii, i);

		submits[at].sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submits[at].pNext				= nullptr;

		submits[at].commandBufferCount	= 1;
		submits[at].pCommandBuffers		= i->ptr();

		submits[at].waitSemaphoreCount	= 0;
		submits[at].pWaitSemaphores		= nullptr;
		submits[at].pWaitDstStageMask	= nullptr;

		submits[at].signalSemaphoreCount	= 0;
		submits[at].pSignalSemaphores	= nullptr;

		zfences.emplace_back(createFence(device));
		fences[at] = *zfences.back();
	}

	for (auto i = ii; i != commandBuffers.end(); ++i)
	{
		auto at = std::distance(ii, i);
		ZQueue queue = i->getParam<ZCommandPool>().getParam<ZQueue>();
		if (vkQueueSubmit(*queue, 1, &submits[at], fences[at]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit command buffer!");
		}
	}

	if (vkWaitForFences(*device, commandBufferCount, fences.data(), VK_TRUE, ~0ull) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to wait for command buffer!");
	}
}

uint32_t findMemoryTypeIndex(ZDevice device, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
{
	auto physicalDevice = device.getParam<ZPhysicalDevice>();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(*physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

ZDeviceMemory createMemory (ZDevice device, const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties)
{
	VkDeviceMemory	memory		= VK_NULL_HANDLE;
	auto			callbacks	= device.getParam<VkAllocationCallbacksPtr>();

	VkExternalMemoryImageCreateInfo ici;
	VkExternalMemoryBufferCreateInfo ibi{};
	ibi.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	UNREF(ici);
	UNREF(ibi);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext				= nullptr; // or VkExternalMemoryBufferCreateInfo, VkExternalMemoryImageCreateInfo
	allocInfo.allocationSize	= requirements.size;
	allocInfo.memoryTypeIndex	= findMemoryTypeIndex(device, requirements.memoryTypeBits, properties);

	VKASSERT(vkAllocateMemory(*device, &allocInfo, callbacks, &memory), "failed to allocate buffer memory!");

	return ZDeviceMemory::create(memory, device, callbacks, properties, requirements.size, nullptr);
}

uint8_t* mapMemory (ZDeviceMemory memory)
{
	const VkMemoryPropertyFlags	props	= memory.getParam<VkMemoryPropertyFlags>();
	ASSERTION(props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	uint32_t		counter	= memory.counter().fetch_add(1);

	if (0u == counter)
	{
		ZDevice			device	= memory.getParam<ZDevice>();
		VkDeviceSize	size	= memory.getParam<VkDeviceSize>();
		uint8_t**		pointer	= &memory.getParamRef<uint8_t*>();
		if (vkMapMemory(*device, *memory, 0, size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(pointer)) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to map memory");
		}
	}

	return memory.getParam<uint8_t*>();
}

void unmapMemory (ZDeviceMemory memory)
{
	uint32_t		counter	= memory.counter();

	if (0u == counter)
	{
		ASSERTION(false);
	}
	else
	{
		if (1u == counter)
		{
			ZDevice	device	= memory.getParam<ZDevice>();
			vkUnmapMemory(*device, *memory);
			memory.getParamRef<uint8_t*>() = nullptr;
		}

		memory.counter().fetch_sub(1);
	}
}

void flushMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkMappedMemoryRange	range{};
	range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.memory	= *memory;
	range.offset	= 0u;
	range.size		= size;

	vkFlushMappedMemoryRanges(*device, 1, &range);
}

void invalidateMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkMappedMemoryRange	range{};
	range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.memory	= *memory;
	range.offset	= 0u;
	range.size		= size;

	vkInvalidateMappedMemoryRanges(*device, 1, &range);
}

ZBuffer	createBuffer (ZDevice device, VkDeviceSize size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	VkBuffer	buffer		= VK_NULL_HANDLE;
	auto		callbacks	= device.getParam<VkAllocationCallbacksPtr>();

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.usage		= usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VKASSERT(vkCreateBuffer(*device, &bufferInfo, callbacks, &buffer), "failed to create buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(*device, buffer, &memRequirements);

	ZDeviceMemory	bufferMemory = createMemory(device, memRequirements, properties);

	VKASSERT2(vkBindBufferMemory(*device, buffer, *bufferMemory, 0));

	return ZBuffer::create(buffer, device, callbacks, bufferInfo, bufferMemory, size);
}

ZBuffer createBuffer (ZDevice device, ZImageView view, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const VkDeviceSize	bufferSize	= computeBufferSize(view);
	return createBuffer(device, bufferSize, usage, properties);
}

ZBuffer createBuffer (ZDevice device, ZImage image, ZBufferUsageFlags usage, uint32_t baseLevel, uint32_t levels, ZMemoryPropertyFlags properties)
{
	const VkImageCreateInfo& ici = image.getParamRef<VkImageCreateInfo>();
	ASSERTION(0 < levels && (baseLevel + levels) <= ici.mipLevels);
	const VkDeviceSize bufferSize = computeBufferSize(image, baseLevel, levels);
	return createBuffer(device, bufferSize, usage, properties);
}

ZBuffer createBuffer (ZDevice device, VkFormat format, uint32_t elements, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const VkDeviceSize bufferSize = computePixelByteWidth(format) * elements;
	return createBuffer(device, bufferSize, usage, properties);
}

uint32_t writeBufferData (ZBuffer buffer, const uint8_t* src, VkDeviceSize size)
{
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	if (VK_WHOLE_SIZE == size || bufferSize < size)
		size = bufferSize;

	VkBufferCopy copy{};
	copy.srcOffset	= 0;
	copy.dstOffset	= 0;
	copy.size		= size;

	return writeBufferData(buffer, src, copy);
}

void flushBuffer (ZBuffer buffer)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & props)
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= 0;
		range.size		= VK_WHOLE_SIZE;
		VKASSERT(vkFlushMappedMemoryRanges(*device, 1, &range), "");
	}
}

uint32_t writeBufferData (ZBuffer buffer, const uint8_t* src, const VkBufferCopy& copy, bool flush)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTION(0 < copy.size);
	ASSERTION(bufferSize > copy.dstOffset);
	ASSERTION(bufferSize >= (copy.dstOffset + copy.size));

	uint8_t* dst = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.dstOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)));
	ASSERTION(dst != nullptr);

	if (flush && (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & props))
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.dstOffset;
		range.size		= copy.size;
		VKASSERT2(vkFlushMappedMemoryRanges(*device, 1, &range));
	}

	std::copy(src, std::next(src, copy.size), dst);
	vkUnmapMemory(*device, *memory);

	return static_cast<uint32_t>(copy.size);
}

uint32_t readBufferData (ZBuffer buffer, uint8_t* dst, VkDeviceSize size)
{
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	if (VK_WHOLE_SIZE == size || bufferSize < size)
		size = bufferSize;

	VkBufferCopy copy{};
	copy.srcOffset	= 0;
	copy.dstOffset	= 0;
	copy.size		= size;

	return readBufferData(buffer, dst, copy);
}

uint32_t readBufferData (ZBuffer buffer, uint8_t* dst, const VkBufferCopy& copy)
{
	ZDevice					device		= buffer.getParam<ZDevice>();
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTION(0 < copy.size);
	ASSERTION(bufferSize > copy.srcOffset);
	ASSERTION(bufferSize >= (copy.srcOffset + copy.size));

	uint8_t* src = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.srcOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&src)));

	if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & props)
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.srcOffset;
		range.size		= copy.size;
		VKASSERT2(vkInvalidateMappedMemoryRanges(*device, 1, &range));
	}

	std::copy(src, std::next(src, copy.size), dst);
	vkUnmapMemory(*device, *memory);

	return static_cast<uint32_t>(copy.size);
}

void copyBufferToBuffer(ZCommandBuffer cmd, ZBuffer src, ZBuffer dst, const VkBufferCopy& region)
{
	auto pool		= cmd.getParam<ZCommandPool>();
	auto queue		= pool.getParam<ZQueue>();
	auto familyIdx	= queue.getParam<uint32_t>();
	auto src_mem	= src.getParam<ZDeviceMemory>();
	auto dst_mem	= dst.getParam<ZDeviceMemory>();
	const bool enableBarriers = src_mem.getParam<VkMemoryPropertyFlags>() & dst_mem.getParam<VkMemoryPropertyFlags>() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (enableBarriers)
	{
	}

	vkCmdCopyBuffer(*cmd, *src, *dst, 1, &region);

	if (enableBarriers)
	{
		familyIdx = VK_QUEUE_FAMILY_IGNORED;

		VkBufferMemoryBarrier bm;
		bm.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bm.pNext = nullptr;
		bm.srcQueueFamilyIndex = familyIdx;
		bm.dstQueueFamilyIndex = familyIdx;
		bm.srcAccessMask = 0;
		bm.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bm.buffer = *dst;
		bm.offset = region.dstOffset;
		bm.size = region.size;

		vkCmdPipelineBarrier(*cmd,
							 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
							 (VkDependencyFlagBits)0,
							 0, nullptr,	// memory
							 1, &bm,
							 0, nullptr);	// image
	}
}

void copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImageView view, VkImageLayout newLayout)
{
	ZImage							image	= view.getParam<ZImage>();
	const VkImageViewCreateInfo&	info	= view.getParamRef<VkImageViewCreateInfo>();
	copyBufferToImage(commandPool, buffer, image, info.subresourceRange.baseMipLevel, info.subresourceRange.levelCount, newLayout);
}

void transitionImage (ZCommandBuffer cmd, ZImage image, VkImageLayout targetLayout,
					  VkAccessFlags			sourceAccess,	VkAccessFlags			destinationAccess,
					  VkPipelineStageFlags	sourceStage,	VkPipelineStageFlags	destinationStage)
{
	auto&					createInfo		= image.getParamRef<VkImageCreateInfo>();

	VkImageMemoryBarrier	barrier			{};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.oldLayout			= createInfo.initialLayout;
	barrier.newLayout			= targetLayout;
	barrier.srcAccessMask		= sourceAccess;
	barrier.dstAccessMask		= destinationAccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.image				= *image;
	barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT; // TODO: It should depend on image format
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= createInfo.mipLevels;
	barrier.subresourceRange.baseArrayLayer	= 0;
	barrier.subresourceRange.layerCount		= createInfo.arrayLayers;

	vkCmdPipelineBarrier(*cmd, sourceStage, destinationStage, VK_DEPENDENCY_BY_REGION_BIT,
						 0, (const VkMemoryBarrier*)nullptr,
						 0, (const VkBufferMemoryBarrier*)nullptr,
						 1, &barrier);

	createInfo.initialLayout	= targetLayout;
}

void copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImage image, uint32_t baseLevel, uint32_t levels, VkImageLayout newLayout)
{
	auto&				createInfo		= image.getParamRef<VkImageCreateInfo>();
	const auto			pixelWidth		= computePixelByteWidth(createInfo.format);
	const uint32_t		effectiveLevels = (INVALID_UINT32 == levels) ? createInfo.mipLevels : levels;
	const VkDeviceSize	bufferSize		= computeBufferSize(image, baseLevel, levels);

	ASSERTION(0 < effectiveLevels && (baseLevel + effectiveLevels) <= createInfo.mipLevels);
	ASSERTION(bufferSize >= buffer.getParam<VkDeviceSize>());

	VkDeviceSize		offset		= 0;
	uint32_t			width		= (createInfo.extent.width >> baseLevel);
	uint32_t			height		= (createInfo.extent.height >> baseLevel);
	auto				shotCommand	= createOneShotCommandBuffer(commandPool);

	transitionImage(shotCommand->commandBuffer, image, newLayout,
					0/*VK_ACCESS_NONE*/, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	VkBufferImageCopy region{};
	for (uint32_t level = 0; level < effectiveLevels; ++level)
	{
		ASSERTION(width > 0 && height > 0);

		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = (baseLevel + level);
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = { width, height, createInfo.extent.depth };

		vkCmdCopyBufferToImage(*shotCommand->commandBuffer, *buffer, *image, newLayout, 1, &region);

		offset += width * height * pixelWidth;
		height /= 2;
		width /= 2;
	}

	createInfo.initialLayout = newLayout;
	image.setParam<VkImageCreateInfo>(createInfo);
}

void copyImageToBuffer (ZCommandPool commandPool, ZImageView view, ZBuffer buffer)
{
	const VkImageViewCreateInfo&	info	= view.getParamRef<VkImageViewCreateInfo>();
	ZImage							image	= view.getParam<ZImage>();
	copyImageToBuffer(commandPool, image, buffer, info.subresourceRange.baseMipLevel, info.subresourceRange.levelCount);
}

void copyImageToBuffer (ZCommandPool commandPool, ZImage image, ZBuffer buffer, uint32_t baseLevel, uint32_t levels)
{
	auto shotCommand = createOneShotCommandBuffer(commandPool);
	transitionImage(shotCommand->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					0/*VK_ACCESS_NONE*/, VK_ACCESS_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	copyImageToBuffer(shotCommand->commandBuffer, image, buffer, baseLevel, levels);
}

void copyImageToBuffer (ZCommandBuffer commandBuffer, ZImage image, ZBuffer buffer, uint32_t baseLevel, uint32_t levels)
{
	const auto			createInfo		= image.getParam<VkImageCreateInfo>();
	const auto			pixelWidth		= computePixelByteWidth(createInfo.format);
	const uint32_t		effectiveLevels = ((~0u) == levels) ? createInfo.mipLevels : levels;
	const VkDeviceSize	bufferSize		= computeBufferSize(image, baseLevel, levels);

	ASSERTION(0 < effectiveLevels && (baseLevel + effectiveLevels) <= createInfo.mipLevels);
	ASSERTION(bufferSize >= buffer.getParam<VkDeviceSize>());

	VkImageSubresourceLayers subresourceTemplate;
	subresourceTemplate.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceTemplate.mipLevel		= 0;
	subresourceTemplate.baseArrayLayer	= 0;
	subresourceTemplate.layerCount		= 1;

	VkBufferImageCopy regionTemplate{};
	regionTemplate.bufferOffset			= 0;
	regionTemplate.bufferRowLength		= 0;
	regionTemplate.bufferImageHeight	= 0;
	regionTemplate.imageSubresource		= subresourceTemplate;
	regionTemplate.imageOffset = { 0, 0, 0 };
	regionTemplate.imageExtent = { 0, 0, 1 };

	{
		VkDeviceSize	bufferOffset	= 0;
		uint32_t		width			= (createInfo.extent.width >> baseLevel);
		uint32_t		height			= (createInfo.extent.height >> baseLevel);

		std::vector<VkBufferImageCopy>	regions(effectiveLevels, regionTemplate);

		for (uint32_t level = 0; level < effectiveLevels; ++level)
		{
			ASSERTION(width > 0 && height > 0);

			VkBufferImageCopy& region = regions[level];

			region.imageSubresource.mipLevel = (baseLevel + level);

			region.imageExtent.width = width;
			region.imageExtent.height = height;

			region.bufferOffset = bufferOffset;
			region.bufferRowLength = 0; // * computePixelByteWidth(createInfo.format);

			bufferOffset += (width * height * pixelWidth);

			height /= 2;
			width /= 2;
		}

		vkCmdCopyImageToBuffer(*commandBuffer, *image, createInfo.initialLayout, *buffer, effectiveLevels, regions.data());
	}
}

int computePixelByteWidth(VkFormat format)
{
	const ZFormatInfo info = getFormatInfo(format);
	return info.pixelByteSize;
}

int computePixelChannelCount(VkFormat format)
{
	const ZFormatInfo info = getFormatInfo(format);
	return info.componentCount;
}

uint32_t sampleFlagsToSampleCount(VkSampleCountFlags flags)
{
	struct
	{
		VkSampleCountFlagBits	bits;
		uint32_t				mask;
	}
	inf[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT,	0x00000001 },
		{ VK_SAMPLE_COUNT_2_BIT,	0x00000002 },
		{ VK_SAMPLE_COUNT_4_BIT,	0x00000004 },
		{ VK_SAMPLE_COUNT_8_BIT,	0x00000008 },
		{ VK_SAMPLE_COUNT_16_BIT,	0x00000010 },
		{ VK_SAMPLE_COUNT_32_BIT,	0x00000020 },
		{ VK_SAMPLE_COUNT_64_BIT,	0x00000040 },
	};
	uint32_t count = 0;
	for (auto i : inf)
	{
		if (flags & i.bits)
			count |= i.mask;
	}
	ASSERTION(count > 0);
	return count;
}

uint32_t computeMipLevelCount(uint32_t width, uint32_t height)
{
	return static_cast<uint32_t>(std::min(std::floor(std::log2(width)), std::floor(std::log2(height)))) + 1;
}

static int computeBufferPixelCount (uint32_t baseLevelWidth, uint32_t baseLevelHeight,
									uint32_t levels, uint32_t layers, VkSampleCountFlags samples)
{
	ASSERTION(baseLevelWidth > 0 && baseLevelHeight > 0 && levels > 0 && layers > 0 && samples != 0);

	const uint32_t sampleCount = sampleFlagsToSampleCount(samples);

	const uint32_t effectiveLevelCount = std::min(levels, computeMipLevelCount(baseLevelWidth, baseLevelHeight));

	uint32_t pixelCount = 0;
	uint32_t tmpWidth = baseLevelWidth;
	uint32_t tmpHeight = baseLevelHeight;
	for (uint32_t level = 0; level < effectiveLevelCount; ++level)
	{
		ASSERTION(tmpWidth > 0 && tmpHeight > 0);
		pixelCount += (tmpWidth * tmpHeight);
		tmpWidth /= 2;
		tmpHeight /= 2;
	}

	const int count = (pixelCount * sampleCount * layers);
	ASSERTION(count > 0);

	return count;
}

VkDeviceSize computeBufferSize (VkFormat format,
								uint32_t baseLevelWidth, uint32_t baseLevelHeight, uint32_t levels,
								uint32_t layers, VkSampleCountFlags samples)
{
	ASSERTION(baseLevelWidth > 0 && baseLevelHeight > 0 && levels > 0 && layers > 0 && samples != 0);

	const uint32_t pixelCount = computeBufferPixelCount(baseLevelWidth, baseLevelHeight, levels, layers, samples);
	const uint32_t pixelWidth = computePixelByteWidth(format);

	return (pixelCount * pixelWidth);
}

int computeBufferPixelCount (ZImage image, uint32_t baseLevel, uint32_t levels)
{
	const auto ici = image.getParam<VkImageCreateInfo>();
	const uint32_t effectiveLevels = ((~0u) == levels) ? ici.mipLevels : levels;
	ASSERTION(0 < effectiveLevels && (baseLevel + effectiveLevels) <= ici.mipLevels);
	return computeBufferPixelCount((ici.extent.width >> baseLevel),
								   (ici.extent.height >> baseLevel),
								   effectiveLevels, ici.arrayLayers, ici.samples);
}

int computeBufferPixelCount (ZImageView view)
{
	ZImage							image		= view.getParam<ZImage>();
	const VkImageCreateInfo&		imageInfo	= image.getParamRef<VkImageCreateInfo>();
	const VkImageViewCreateInfo&	viewInfo	= view.getParamRef<VkImageViewCreateInfo>();
	const VkImageSubresourceRange&	range		= viewInfo.subresourceRange;

	return computeBufferPixelCount(	(imageInfo.extent.width << range.baseMipLevel),
									(imageInfo.extent.height << range.baseMipLevel),
									range.levelCount, 1/*layers*/, imageInfo.samples);
}

VkDeviceSize computeBufferSize (ZImage image, uint32_t baseLevel, uint32_t levels)
{
	const auto ici = image.getParam<VkImageCreateInfo>();
	const uint32_t effectiveLevels = ((~0u) == levels) ? ici.mipLevels : levels;
	ASSERTION(0 < effectiveLevels && (baseLevel + effectiveLevels) <= ici.mipLevels);
	return computeBufferSize(ici.format,
							 (ici.extent.width >> baseLevel),
							 (ici.extent.height >> baseLevel),
							 effectiveLevels, 1, ici.samples);
}

VkDeviceSize computeBufferSize (ZImageView view)
{
	ZImage							image		= view.getParam<ZImage>();
	const VkImageCreateInfo&		imageInfo	= image.getParamRef<VkImageCreateInfo>();
	const VkImageViewCreateInfo&	viewInfo	= view.getParamRef<VkImageViewCreateInfo>();
	const VkImageSubresourceRange&	range		= viewInfo.subresourceRange;

	const VkDeviceSize				bufferSize	= computeBufferSize(viewInfo.format,
																	(imageInfo.extent.width << range.baseMipLevel),
																	(imageInfo.extent.height << range.baseMipLevel),
																	range.levelCount, 1/*layers*/, imageInfo.samples);
	return bufferSize;
}

void computeBufferRange(ZImage image, uint32_t level, uint32_t& bufferSize, uint32_t pixelCount)
{
	ASSERTMSG(false, "Not implemented yet");
	UNREF(image);
	UNREF(level);
	UNREF(bufferSize);
	UNREF(pixelCount);
}

bool pointInTriangle2D(const Vec3& p, const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
	float s = p0.y() * p2.x() - p0.x() * p2.y() + (p2.y() - p0.y()) * p.x() + (p0.x() - p2.x()) * p.y();
	float t = p0.x() * p1.y() - p0.y() * p1.x() + (p0.y() - p1.y()) * p.x() + (p1.x() - p0.x()) * p.y();

	if ((s < 0) != (t < 0))
		return false;

	float a = -p1.y() * p2.x() + p0.y() * (p2.x() - p1.x()) + p0.x() * (p1.y() - p2.y()) + p1.x() * p2.y();

	return a < 0 ?
		(s <= 0 && s + t >= a) :
		(s >= 0 && s + t <= a);
}

} // namespace vtf
