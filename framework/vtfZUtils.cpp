#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{

VkClearValue makeClearColor (const Vec4& color)
{
	return {{{ color[0], color[1], color[2], color[3] }}};
}

std::ostream& operator<<(std::ostream& str, const Version& v)
{
	str << "(" << v.nmajor << ", " << v.nminor << ", " << v.npatch << ")";
	return str;
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

Version getVulkanImplVersion (std::optional<ZInstance> instance)
{
	Version version(1, 0);
	VkInstance i = instance.has_value() ? **instance : VK_NULL_HANDLE;
	PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVerion =
		reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(i, "vkEnumerateInstanceVersion"));
	if (pfnEnumerateInstanceVerion)
	{
		uint32_t tmp = 0;
		(*pfnEnumerateInstanceVerion)(&tmp);
		version.update(tmp);
	}
	return version;
}

ZInstance		createInstance (const char*							appName,
								VkAllocationCallbacksPtr			callbacks,
								const strings&						desiredLayers,
								const strings&						desiredExtensions,
								VkDebugUtilsMessengerEXT*			pMessenger,
								void*								pMessengerUserData,
								add_ptr<VkDebugReportCallbackEXT>	pReport,
								void*								pReportUserData,
								uint32_t							apiVersion,
								bool								enableDebugPrintf)
{
	ZInstance			instance			(callbacks, apiVersion, {}, {});
	add_ref<strings>	requiredLayers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>().data;
	add_ref<strings>	availableExtensions	= instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>().data;

	requiredLayers = desiredLayers;
	if (enableDebugPrintf)
		requiredLayers.push_back("VK_LAYER_KHRONOS_validation");

	strings availableLayers;
	if (requiredLayers.size())
	{
		availableLayers = enumerateInstanceLayers();
		if (!containsAllString(requiredLayers, availableLayers))
			ASSERTMSG(false, "All required layer(s) must match to available instance layer(s)!!!");
	}

	availableExtensions	= enumerateInstanceExtensions(requiredLayers);

	ASSERTMSG(containsAllString(desiredExtensions, availableExtensions), "All required extension(s) must match available instance extension(s)");

	VkValidationFeatureEnableEXT						enabledValidationFeature = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
	VkValidationFeaturesEXT								validationFeatures{};
	validationFeatures.sType							= VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
	validationFeatures.pNext							= nullptr;
	validationFeatures.pEnabledValidationFeatures		= &enabledValidationFeature;
	validationFeatures.enabledValidationFeatureCount	= 1;
	validationFeatures.pDisabledValidationFeatures		= nullptr;
	validationFeatures.disabledValidationFeatureCount	= 0;

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
	VkDebugReportCallbackCreateInfoEXT debugReportInfo{};
	const std::string debugUtilsExtName(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		// "VK_EXT_debug_utils"
	const std::string debugReportExtName(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// "VK_EXT_debug_report"
	const bool debugMessengerEnabled	= ((containsString(debugUtilsExtName, desiredExtensions) || pMessenger != nullptr)
										&& containsString(debugUtilsExtName, availableExtensions));
	const bool debugReportEnabled		= enabledValidationFeature
										&& ((containsString(debugReportExtName, desiredExtensions) || pReport != nullptr)
										&& containsString(debugReportExtName, availableExtensions));

	void* pNext = nullptr, **p2pNext = &pNext;

	if (debugMessengerEnabled)
	{
		getDebugCreateInfo(debugMessengerInfo, pMessengerUserData, nullptr, enableDebugPrintf);
	}
	else
	{
		removeStrings({ debugUtilsExtName }, availableExtensions);
	}

	if (debugReportEnabled)
	{
		getDebugCreateInfo(debugReportInfo, pReportUserData, nullptr, enableDebugPrintf);
		*p2pNext = &validationFeatures;
		p2pNext = (void**)&validationFeatures.pNext;
	}
	else
	{
		removeStrings({ debugReportExtName }, availableExtensions);
	}

	VkApplicationInfo appInfo{};
	appInfo.sType						= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName			= appName;
	appInfo.applicationVersion			= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName					= nullptr;
	appInfo.engineVersion				= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion					= apiVersion;	// default VK_API_VERSION_1_0

	std::vector<const char*>			v_layerNames(to_cstrings(requiredLayers));
	std::vector<const char*>			v_extensions(to_cstrings(availableExtensions));

	VkInstanceCreateInfo createInfo{};
	createInfo.sType					= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext					= pNext;
	createInfo.pApplicationInfo			= &appInfo;

	createInfo.enabledExtensionCount	= static_cast<uint32_t>(v_extensions.size());
	createInfo.ppEnabledExtensionNames	= v_extensions.size() ? v_extensions.data() : nullptr;
	createInfo.enabledLayerCount		= static_cast<uint32_t>(v_layerNames.size());
	createInfo.ppEnabledLayerNames		= v_layerNames.size() ? v_layerNames.data() : nullptr;

	VKASSERT(vkCreateInstance(&createInfo, callbacks, instance.setter()), "Failed to create instance!");

	if (debugMessengerEnabled) createDebugMessenger(instance, callbacks, debugMessengerInfo, *pMessenger);
	if (debugReportEnabled) createDebugReport(instance, callbacks, debugReportInfo, *pReport);

	if (getGlobalAppFlags().verbose)
	{
		Version nullApiVersion = getVulkanImplVersion();
		Version clientApiVersion = Version::fromUint(apiVersion);
		Version vulkanApiVersion = getVulkanImplVersion(instance);
		std::cout << "[APP] Trying to create versioned instance: " << clientApiVersion << std::endl;
		std::cout << "[APP] Vulkan Null Instance Version:        " << nullApiVersion << std::endl;
		std::cout << "[APP] Vulkan Implementation Version:       " << vulkanApiVersion << std::endl;
	}

	return instance;
}

ZPhysicalDevice	getPhysicalDeviceByIndex (ZInstance instance, uint32_t physicalDeviceIndex)
{
	std::vector<VkPhysicalDevice> devices;
	const uint32_t				deviceCount = enumeratePhysicalDevices(*instance, devices);
	ASSERTMSG(physicalDeviceIndex < deviceCount, "Failed to find GPUs with Vulkan support!");
	VkPhysicalDevice			physDevice	= devices.at(physicalDeviceIndex);
	add_cref<strings>			layers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>().data;
	const strings				extensions	= enumerateDeviceExtensions(devices.at(physicalDeviceIndex), layers);
	VkPhysicalDeviceProperties	deviceProps	{};
	vkGetPhysicalDeviceProperties(physDevice, &deviceProps);
	return ZPhysicalDevice::create(physDevice, instance.getParam<VkAllocationCallbacksPtr>(), instance, physicalDeviceIndex, extensions, deviceProps);
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

	uint32_t							presentQueueIndex	= INVALID_UINT32;
	uint32_t							physicalDeviceIndex	= INVALID_UINT32;
	add_cref<strings>					layers				= instance.getParamRef<ZDistType<RequiredLayers, strings>>().data;
	ZPhysicalDevice						physicalDevice		(instance.getParam<VkAllocationCallbacksPtr>(), instance, physicalDeviceIndex,
																{/*availableExtensions*/}, {/*physicalDeviceProperties*/});
	add_ref<strings>					availableExtensions	= physicalDevice.getParamRef<strings>();
	add_ref<VkPhysicalDeviceProperties>	deviceProperties	= physicalDevice.getParamRef<VkPhysicalDeviceProperties>();
	const strings						extensionToVerify	{ VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	const strings						extensionToRemove	{ VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };

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
			if (containsAllString(availableExtensions, extensionToVerify))
			{
				removeStrings(extensionToRemove, availableExtensions);
			}
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
		if (containsAllString(availableExtensions, extensionToVerify))
		{
			removeStrings(extensionToRemove, availableExtensions);
		}
		removeStrings(extensionToRemove, availableExtensions);
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

	vkGetPhysicalDeviceProperties(result, &deviceProperties);
	physicalDevice.setParam<uint32_t>(physicalDeviceIndex);
	physicalDevice.replace(result);

	return physicalDevice;
}

ZDevice createLogicalDevice	(ZPhysicalDevice							physDevice,
							 const std::map<VkQueueFlagBits, uint32_t>&	queuesToIndices,
							 GetEnabledFeaturesCB						onGetEnabledFeatures,
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

	VkPhysicalDeviceFeatures2	features	{};
	ZInstance					instance	= physDevice.getParam<ZInstance>();
	VkAllocationCallbacksPtr	callbacks	= instance.getParam<VkAllocationCallbacksPtr>();
	std::vector<const char*>	layers		(to_cstrings(instance.getParamRef<ZDistType<RequiredLayers,strings>>().data));

	strings	availableDeviceExtensions(physDevice.getParamRef<strings>());
	if (onGetEnabledFeatures)	features	= onGetEnabledFeatures(physDevice, availableDeviceExtensions);
	std::vector<const char*>	extensions	(to_cstrings(availableDeviceExtensions));

	VkDeviceCreateInfo createInfo{};
	createInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.queueCreateInfoCount		= static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos		= queueCreateInfos.data();

	createInfo.pNext					= onGetEnabledFeatures ? features.pNext : nullptr;
	createInfo.pEnabledFeatures			= onGetEnabledFeatures ? &features.features : nullptr;

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
			handles[make_unsigned(std::distance(b, i))] = **i;
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
			handles[make_unsigned(std::distance(b, i))] = **i;
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
			handles[make_unsigned(std::distance(b, i))] = **i;
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
			handles[make_unsigned(std::distance(b, i))] = **i;
		}
		VKASSERT2(vkResetFences(*device, count, handles.data()));
	}
}

ZSemaphore createSemaphore (ZDevice device)
{
	VkSemaphore handle = VK_NULL_HANDLE;
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();

	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semInfo.pNext = nullptr;
	semInfo.flags = VkSemaphoreCreateFlags(0);

	VKASSERT2(vkCreateSemaphore(*device, &semInfo, callbacks, &handle));

	return ZSemaphore::create(handle, device, callbacks);
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

ZBuffer	createBuffer (ZDevice device, const ExplicitWrapper<VkDeviceSize>& size, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
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

	return ZBuffer::create(buffer, device, callbacks, bufferInfo, bufferMemory, size);
}

ZBuffer createBuffer (ZDevice device, VkFormat format, uint32_t elements, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const VkDeviceSize bufferSize = make_unsigned(computePixelByteWidth(format)) * elements;
	return createBuffer(device, makeExplicitWrapper(bufferSize), usage, properties);
}

ZBuffer createBuffer (ZImage image, ZBufferUsageFlags usage, ZMemoryPropertyFlags properties)
{
	const VkDeviceSize bufferSize = computeBufferSize(image);
	return createBuffer(image.getParam<ZDevice>(), makeExplicitWrapper(bufferSize), usage, properties);
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

uint32_t writeBufferData (ZBuffer buffer, const uint8_t* src, const VkBufferCopy& copy, bool flush)
{
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	if ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		return 0;
	}
	ZDevice					device = buffer.getParam<ZDevice>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTION(0 < copy.size);
	ASSERTION(bufferSize > copy.dstOffset);
	ASSERTION(bufferSize >= (copy.dstOffset + copy.size));

	uint8_t* dst = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.dstOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&dst)));
	ASSERTION(dst != nullptr);

	std::copy(src, std::next(src, make_signed(copy.size)), dst);

	if (flush && ((props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
	{
		VkMappedMemoryRange	range{};
		range.sType		= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory	= *memory;
		range.offset	= copy.dstOffset;
		range.size		= copy.size;
		VKASSERT2(vkFlushMappedMemoryRanges(*device, 1, &range));
	}

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
	ZDeviceMemory			memory		= buffer.getParam<ZDeviceMemory>();
	VkMemoryPropertyFlags	props		= memory.getParam<VkMemoryPropertyFlags>();
	if ((props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		return 0;
	}
	ZDevice					device = buffer.getParam<ZDevice>();
	const VkDeviceSize		bufferSize	= buffer.getParam<VkDeviceSize>();

	ASSERTION(0 < copy.size);
	ASSERTION(bufferSize > copy.srcOffset);
	ASSERTION(bufferSize >= (copy.srcOffset + copy.size));

	uint8_t* src = nullptr;
	VKASSERT2(vkMapMemory(*device, *memory, copy.srcOffset, copy.size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(&src)));

	if ((props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
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

// TODO: Why below are commented?
//void copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImageView view, VkImageLayout newLayout)
//{
//	ZImage							image	= view.getParam<ZImage>();
//	const VkImageViewCreateInfo&	info	= view.getParamRef<VkImageViewCreateInfo>();
//	copyBufferToImage(commandPool, buffer, image, info.subresourceRange.baseMipLevel, info.subresourceRange.levelCount, newLayout);
//}

//void copyBufferToImage (ZCommandPool commandPool, ZBuffer buffer, ZImage image, uint32_t baseLevel, uint32_t levels, VkImageLayout newLayout)
//{
//	auto&				createInfo		= image.getParamRef<VkImageCreateInfo>();
//	const uint32_t		pixelWidth		= make_unsigned(computePixelByteWidth(createInfo.format));
//	const uint32_t		effectiveLevels = (INVALID_UINT32 == levels) ? createInfo.mipLevels : levels;
//	const VkDeviceSize	bufferSize		= computeBufferSize(image, baseLevel, levels);

//	ASSERTION(0 < effectiveLevels && (baseLevel + effectiveLevels) <= createInfo.mipLevels);
//	ASSERTION(bufferSize >= buffer.getParam<VkDeviceSize>());

//	VkDeviceSize		offset		= 0;
//	uint32_t			width		= (createInfo.extent.width >> baseLevel);
//	uint32_t			height		= (createInfo.extent.height >> baseLevel);
//	auto				shotCommand	= createOneShotCommandBuffer(commandPool);

//	transitionImage(shotCommand->commandBuffer, image, newLayout,
//					0/*VK_ACCESS_NONE*/, VK_ACCESS_TRANSFER_WRITE_BIT,
//					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

//	VkBufferImageCopy region{};
//	for (uint32_t level = 0; level < effectiveLevels; ++level)
//	{
//		ASSERTION(width > 0 && height > 0);

//		region.bufferOffset = offset;
//		region.bufferRowLength = 0;
//		region.bufferImageHeight = 0;
//		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//		region.imageSubresource.mipLevel = (baseLevel + level);
//		region.imageSubresource.baseArrayLayer = 0;
//		region.imageSubresource.layerCount = 1;
//		region.imageOffset = {0, 0, 0};
//		region.imageExtent = { width, height, createInfo.extent.depth };

//		vkCmdCopyBufferToImage(*shotCommand->commandBuffer, *buffer, *image, newLayout, 1, &region);

//		offset += width * height * pixelWidth;
//		height /= 2;
//		width /= 2;
//	}

//	createInfo.initialLayout = newLayout;
//	image.setParam<VkImageCreateInfo>(createInfo);
//}

static uint32_t computeBufferPixelCount (const ExplicitWrapper<uint32_t>& zeroLevelWidth,
										 const ExplicitWrapper<uint32_t>& zeroLevelHeight,
										 uint32_t startLevel, uint32_t levelCount, uint32_t layerCount)
{
	ASSERTION(zeroLevelWidth > 0 && zeroLevelHeight > 0 && levelCount > 0 && layerCount > 0);

	uint32_t tmpWidth = zeroLevelWidth >> startLevel;
	uint32_t tmpHeight = zeroLevelHeight >> startLevel;
	ASSERTION(tmpWidth > 0 && tmpHeight > 0);

	const uint32_t effectiveLevelCount = std::min(levelCount, computeMipLevelCount(tmpWidth, tmpHeight));

	uint32_t pixelCount = 0;
	for (uint32_t level = 0; level < effectiveLevelCount; ++level)
	{
		ASSERTION(tmpWidth > 0 && tmpHeight > 0);
		pixelCount += (tmpWidth * tmpHeight);
		tmpWidth /= 2;
		tmpHeight /= 2;
	}

	const uint32_t count = (pixelCount * layerCount);
	ASSERTION(count != 0);

	return count;
}

uint32_t computeBufferPixelCount (ZImage image)
{
	add_cref<VkImageCreateInfo> ici = image.getParamRef<VkImageCreateInfo>();
	return computeBufferPixelCount(makeExplicitWrapper(ici.extent.width),
								   makeExplicitWrapper(ici.extent.height),
								   0, ici.mipLevels, ici.arrayLayers);
}

VkDeviceSize computeBufferSize (ZImage image)
{
	add_cref<VkImageCreateInfo> ici = image.getParamRef<VkImageCreateInfo>();
	const uint32_t pixelWidth = computePixelByteWidth(ici.format);
	return computeBufferPixelCount(image) * pixelWidth;
}

uint32_t computeBufferPixelCount (ZImageView view)
{
//	ZImage							image		= view.getParam<ZImage>();
//	const VkImageCreateInfo&		imageInfo	= image.getParamRef<VkImageCreateInfo>();
//	const VkImageViewCreateInfo&	viewInfo	= view.getParamRef<VkImageViewCreateInfo>();
//	const VkImageSubresourceRange&	range		= viewInfo.subresourceRange;

//	return computeBufferPixelCount(	(imageInfo.extent.width << range.baseMipLevel),
//									(imageInfo.extent.height << range.baseMipLevel),
//									range.levelCount, 1/*layers*/, imageInfo.samples);
	UNREF(view);
	ASSERTION(false);
	return 0;
}

VkDeviceSize computeBufferSize (ZImageView view)
{
	ZImage								image		= view.getParam<ZImage>();
	add_cref<VkImageCreateInfo>			imageInfo	= image.getParamRef<VkImageCreateInfo>();
	add_cref<VkImageViewCreateInfo>		viewInfo	= view.getParamRef<VkImageViewCreateInfo>();
	add_cref<VkImageSubresourceRange>	range		= viewInfo.subresourceRange;

	const VkDeviceSize				bufferSize	= computeBufferSize(viewInfo.format,
																	imageInfo.extent.width, imageInfo.extent.height,
																	range.baseMipLevel, range.levelCount,
																	(range.layerCount - range.baseArrayLayer));
	return bufferSize;
}


VkDeviceSize computeBufferSize (VkFormat format,
								uint32_t imageWidth, uint32_t imageHeight,
								uint32_t baseLevel, uint32_t levels,
								uint32_t layers)
{
	ASSERTION(imageWidth > 0 && imageHeight > 0 && levels > 0 && layers > 0);
	const uint32_t pixelCount = computeBufferPixelCount(
				makeExplicitWrapper(imageWidth), makeExplicitWrapper(imageHeight),
				baseLevel, levels, layers);
	const uint32_t pixelWidth = computePixelByteWidth(format);
	return (pixelCount * pixelWidth);
}

VkBufferMemoryBarrier makeBufferMemoryBarrier	(ZBuffer buffer,
												 VkAccessFlags srcAccess, VkAccessFlags dstAccess,
												 VkDeviceSize size)
{
	const VkDeviceSize	availableSize	= buffer.getParam<VkDeviceSize>();
	if (size == INVALID_UINT64)
		size = availableSize;
	else
	{
		ASSERTION(size <= availableSize);
	}

	VkBufferMemoryBarrier b{};
	b.sType					= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	b.pNext					= nullptr;
	b.srcAccessMask			= srcAccess;
	b.dstAccessMask			= dstAccess;
	b.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	b.buffer				= *buffer;
	b.offset				= 0;
	b.size					= size;

	return b;
}

void computeBufferRange (ZImage image, uint32_t level, uint32_t& bufferSize, uint32_t pixelCount)
{
	UNREF(image);
	UNREF(level);
	UNREF(bufferSize);
	UNREF(pixelCount);
	ASSERT_NOT_IMPLEMENTED();
}

bool pointInTriangle2D (const Vec2& p, const Vec2& p0, const Vec2& p1, const Vec2& p2, bool bar)
{
	UNREF(bar); // TODO:
	float s = p0.y() * p2.x() - p0.x() * p2.y() + (p2.y() - p0.y()) * p.x() + (p0.x() - p2.x()) * p.y();
	float t = p0.x() * p1.y() - p0.y() * p1.x() + (p0.y() - p1.y()) * p.x() + (p1.x() - p0.x()) * p.y();

	if ((s < 0) != (t < 0))
		return false;

	float a = -p1.y() * p2.x() + p0.y() * (p2.x() - p1.x()) + p0.x() * (p1.y() - p2.y()) + p1.x() * p2.y();

	return a < 0 ?
		(s <= 0 && s + t >= a) :
		(s >= 0 && s + t <= a);
}

bool pointInTriangle2D (const Vec3& p, const Vec3& p0, const Vec3& p1, const Vec3& p2, bool bar)
{
	return pointInTriangle2D(Vec2().assign(p), Vec2().assign(p0), Vec2().assign(p1), Vec2().assign(p2), bar);
}

bool pointInTriangle2D (const Vec4& p, const Vec4& p0, const Vec4& p1, const Vec4& p2, bool bar)
{
	return pointInTriangle2D(Vec2().assign(p), Vec2().assign(p0), Vec2().assign(p1), Vec2().assign(p2), bar);
}

} // namespace vtf
