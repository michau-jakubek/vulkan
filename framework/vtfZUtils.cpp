#include "vtfVkUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"
#include "vtfThreadSafeLogger.hpp"

void releaseQueue(const QueueParams& params)
{
	const uint32_t	queueFamilyIndex	= std::get<ZDistType<QueueFamilyIndex, uint32_t>>(params);
	const uint32_t	queueIndex			= std::get<ZDistType<QueueIndex, uint32_t>>(params);
	ZDevice			device				= std::get<ZDevice>(params);
	add_ref<std::vector<ZDeviceQueueCreateInfo>> infos = device.getParamRef<std::vector<ZDeviceQueueCreateInfo>>();
	add_ref<std::bitset<32>> queues		= infos.at(queueFamilyIndex).queues;

	queues.set(queueIndex);

	if (getGlobalAppFlags().verbose)
	{

		const bool		surfaceSupported	= std::get<bool>(params);
		//const VkQueueFlags	queueFlags		= std::get<ZDistType<QueueFlags, VkQueueFlags>>(params);

		std::cout << "[INFO] Releasing ZQueue, "
				  << "familyIndex: " << queueFamilyIndex << ", "
				  << "queueIndex: " << queueIndex << ", "
				  << "surfaceSupported: " << std::boolalpha << surfaceSupported << std::noboolalpha
				  << std::endl;
	}
}

namespace vtf
{

std::ostream& operator<<(std::ostream& str, const Version& v)
{
	str << "(" << v.nmajor << ", " << v.nminor << ", " << v.npatch << ")";
	return str;
}

ZShaderModule createShaderModule(ZDevice device, VkShaderStageFlagBits stage, const std::string& code)
{
	VkShaderModuleCreateInfo createInfo = makeVkStruct();
	createInfo.codeSize	= code.length();
	createInfo.pCode	= reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule				shaderModule	= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	if (vkCreateShaderModule(*device, &createInfo, callbacks, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return ZShaderModule::create(shaderModule, device, callbacks, stage);
}

ZShaderModule createShaderModule(ZDevice device, VkShaderStageFlagBits stage, const std::vector<unsigned char>& code)
{
	auto readMagicNumber = [](const std::vector<unsigned char>& s) -> uint32_t
	{
		return *(const uint32_t*)(s.data());
	};
	auto changeMagicNumber = [](uint32_t magic, std::vector<unsigned char>& s) -> void
	{
		*(uint32_t*)(s.data()) = magic;
	};

	VkShaderModuleCreateInfo createInfo = makeVkStruct();
	createInfo.codeSize	= code.size();
	createInfo.pCode	= reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule				shaderModule	= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();

	std::vector<unsigned char> code2;
	if (readMagicNumber(code) == 0)
	{
		add_ref<Logger> logger = device.getParam<ZPhysicalDevice>().getParam<ZInstance>().getParamRef<Logger>();
		if (getGlobalAppFlags().verbose)
		{
			logger << "Magic number was: 0x" << std::hex << readMagicNumber(code) << std::dec << std::endl;
		}
		code2 = code;
		changeMagicNumber(0x07230203, code2);
		createInfo.pCode	= reinterpret_cast<const uint32_t*>(code2.data());
		if (getGlobalAppFlags().verbose)
		{
			logger << "Magic number is: 0x" << std::hex << readMagicNumber(code2) << std::dec << std::endl;
		}
	}

	VKASSERT2(vkCreateShaderModule(*device, &createInfo, callbacks, &shaderModule));

	return ZShaderModule::create(shaderModule, device, callbacks, stage);
}

ZFramebuffer createFramebuffer (ZRenderPass renderPass, add_cref<VkExtent2D> size, const std::vector<ZImageView>& attachments)
{
	return createFramebuffer(renderPass, size.width, size.height, attachments);
}

ZFramebuffer createFramebuffer (ZRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<ZImageView>& attachments)
{
	const uint32_t	renderAttachmentCount	= renderPass.getParam<ZDistType<AttachmentCount, uint32_t>>();
	const uint32_t	attachmentCount			= data_count(attachments);
	const ZDevice	device					= renderPass.getParam<ZDevice>();
	const auto		allocationCallbacks		= device.getParam<VkAllocationCallbacksPtr>();

	ASSERTMSG(attachmentCount > 0, "Attachment count must be greater than zero");
	ASSERTMSG((renderAttachmentCount <= attachmentCount), "RenderPass attachments must not exceed framebuffer's attachment count being created");
	add_cref<VkPhysicalDeviceLimits> limits = deviceGetPhysicalLimits(device);
	ASSERTION(attachmentCount <= limits.maxFragmentOutputAttachments && attachmentCount <= limits.maxColorAttachments);

	VkFramebuffer	framebuffer = VK_NULL_HANDLE;

	std::vector<VkImageView>	views(attachmentCount);
	for (uint32_t i = 0; i < attachmentCount; ++i)
	{
		views[i] = *attachments[i];
	}

	VkFramebufferCreateInfo framebufferInfo = makeVkStruct();
	framebufferInfo.renderPass		= *renderPass;
	framebufferInfo.attachmentCount	= attachmentCount;
	framebufferInfo.pAttachments	= views.data();
	framebufferInfo.width			= width;
	framebufferInfo.height			= height;
	framebufferInfo.layers			= 1;

	VKASSERT2(vkCreateFramebuffer(*device, &framebufferInfo, allocationCallbacks, &framebuffer));

	return ZFramebuffer::create(framebuffer, device, allocationCallbacks, width, height, renderPass, attachments);
}

ZRenderPassBeginInfo::ZRenderPassBeginInfo (ZCommandBuffer cmd, ZFramebuffer framebuffer, uint32_t subpass)
	: m_cmdBuffer			(cmd)
	, m_framebuffer			(framebuffer)
	, m_renderPass			(framebuffer.getParam<ZRenderPass>())
	, m_subpass				(subpass)
{
}
ZRenderPassBeginInfo::ZRenderPassBeginInfo (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer, uint32_t subpass)
	: m_cmdBuffer			(cmd)
	, m_framebuffer			(framebuffer)
	, m_renderPass			(renderPass)
	, m_subpass				(subpass)
{
}
const VkRenderPassBeginInfo ZRenderPassBeginInfo::operator ()() const
{
	const uint32_t width = m_framebuffer.getParam<ZDistType<Width, uint32_t>>();
	const uint32_t height = m_framebuffer.getParam<ZDistType<Height, uint32_t>>();
	add_cref<std::vector<VkClearValue>> clearValues = m_renderPass.getParamRef<std::vector<VkClearValue>>();

	VkRenderPassBeginInfo	info = makeVkStruct();
	info.renderPass		= *m_renderPass;
	info.framebuffer	= *m_framebuffer;
	info.renderArea.offset.x	= 0u;
	info.renderArea.offset.y	= 0u;
	info.renderArea.extent.width	= width;
	info.renderArea.extent.height	= height;
	info.clearValueCount	= data_count(clearValues);
	info.pClearValues		= data_or_null(clearValues);

	return info;
}

ZRenderPass	createRenderPassImpl (ZDevice device, void* pNext,
								  add_cref<std::vector<VkFormat>> colorFormats,
								  std::optional<std::vector<VkClearValue>> clearColors,
								  VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
								  add_cref<std::vector<ZSubpassDependency>> deps)
{
	const uint32_t attachmentCount = data_count(colorFormats);
	ASSERTMSG(colorFormats.size(), "pColorAttachments must contain at least one element");
	ASSERTMSG(deviceGetPhysicalLimits(device).maxColorAttachments > attachmentCount,
			  "Attachments you want exceed maxAttachmens");

	add_ptr<std::vector<VkClearValue>> pClearColors = clearColors.has_value() ? &clearColors.value() : nullptr;
	const uint32_t clearColorCount = pClearColors ? uint32_t(clearColors->size()) : 0u;

	VkAttachmentDescription attachmentTemplate{};
	attachmentTemplate.flags			= VkAttachmentDescriptionFlags(0);
	attachmentTemplate.format			= VK_FORMAT_UNDEFINED;
	attachmentTemplate.samples			= VK_SAMPLE_COUNT_1_BIT;
	attachmentTemplate.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentTemplate.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	attachmentTemplate.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentTemplate.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentTemplate.initialLayout	= initialColorLayout;
	attachmentTemplate.finalLayout		= finalColorLayout;

	VkAttachmentReference	referenceTemplate{};
	referenceTemplate.attachment	= 0u;
	referenceTemplate.layout		= finalColorLayout;

	std::vector<VkAttachmentDescription>	descriptions(attachmentCount, attachmentTemplate);
	std::vector<VkAttachmentReference>		references(attachmentCount, referenceTemplate);

	for (uint32_t attachment = 0; attachment < attachmentCount; ++attachment)
	{
		ASSERTMSG(colorFormats[attachment] != VK_FORMAT_UNDEFINED, "Format must not be VK_FORMAT_UNDEFINED");
		add_ref<VkAttachmentDescription> desc = descriptions.at(attachment);
		desc.format	= colorFormats.at(attachment);
		if (attachment < clearColorCount)
		{
			if (0u != pClearColors->at(attachment).color.uint32[3])
			{
				desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			else
			{
				desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				desc.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
				desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}
		else if (initialColorLayout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		references.at(attachment).attachment = attachment;
	}

	VkSubpassDescription subpassTemplate;
	subpassTemplate.flags					= VkSubpassDescriptionFlags(0);
	subpassTemplate.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassTemplate.colorAttachmentCount	= attachmentCount;
	subpassTemplate.pColorAttachments		= references.data();
	subpassTemplate.pDepthStencilAttachment	= nullptr;
	subpassTemplate.inputAttachmentCount	= 0;
	subpassTemplate.pInputAttachments		= nullptr;
	subpassTemplate.preserveAttachmentCount	= 0;
	subpassTemplate.pPreserveAttachments	= nullptr;
	subpassTemplate.pResolveAttachments		= nullptr;

	std::vector<VkSubpassDependency> subpassDeps(deps.size());

	// beg, self,	self	self,	end
	// x-0	0-0		1-1		2-2		2-y

	// beg,	self,	def,	def,	self,	end
	// x-0	0-0		0-1		1-2		2-2		2-y

	uint32_t	dependencyIdx	= 0u;
	uint32_t	selfDependencyCount = 0;
	// There must be at least one subpass
	uint32_t	subpassCount = 1u;
	for (add_cref<ZSubpassDependency> dep : deps)
	{
		VkSubpassDependency sd = dep();
		switch (dep.getType())
		{
		case ZSubpassDependency::Begin:
			sd.srcSubpass = VK_SUBPASS_EXTERNAL;
			sd.dstSubpass = subpassCount - 1u;
			break;
		case ZSubpassDependency::Self:
			sd.srcSubpass = subpassCount - 1u + selfDependencyCount;
			sd.dstSubpass = subpassCount - 1u + selfDependencyCount;
			selfDependencyCount = selfDependencyCount + 1u;
			if (selfDependencyCount > 1u)
				subpassCount = subpassCount + 1u;
			break;
		case ZSubpassDependency::Between:
			sd.srcSubpass = subpassCount - 1u;
			sd.dstSubpass = subpassCount;
			subpassCount = subpassCount + 1u;
			break;
		case ZSubpassDependency::End:
			sd.srcSubpass = subpassCount - 1u;
			sd.dstSubpass = VK_SUBPASS_EXTERNAL;
			break;
		}
		subpassDeps[dependencyIdx++] = sd;
	}

	std::vector<VkSubpassDescription> subpasses(subpassCount, subpassTemplate);

	if (pNext)
	{
		((VkRenderPassMultiviewCreateInfo*)pNext)->subpassCount = data_count(subpasses);
		((VkRenderPassMultiviewCreateInfo*)pNext)->dependencyCount = data_count(subpassDeps);
	}

	VkRenderPassCreateInfo renderPassInfo = makeVkStruct();
	renderPassInfo.pNext			= pNext;
	renderPassInfo.flags			= VkRenderPassCreateFlags(0);
	renderPassInfo.attachmentCount	= attachmentCount;
	renderPassInfo.pAttachments		= descriptions.data();
	renderPassInfo.subpassCount		= data_count(subpasses);
	renderPassInfo.pSubpasses		= data_or_null(subpasses);
	renderPassInfo.dependencyCount	= data_count(subpassDeps);
	renderPassInfo.pDependencies	= data_or_null(subpassDeps);


	const VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();
	ZRenderPass	renderPass = ZRenderPass::create(VK_NULL_HANDLE, device, callbacks,
												 attachmentCount, renderPassInfo.subpassCount,
												 {/*clearValues*/}, false, finalColorLayout);
	VKASSERT2(vkCreateRenderPass(*device, &renderPassInfo, callbacks, renderPass.setter()));

	if (pClearColors)
	{
		add_ref<std::vector<VkClearValue>> clearValues = renderPass.getParamRef<std::vector<VkClearValue>>();
		clearValues.insert(clearValues.end(), pClearColors->begin(), pClearColors->end());
	}

	return renderPass;
}

ZRenderPass	createColorRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
								   std::optional<std::vector<VkClearValue>> clearColors,
								   VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
								   std::initializer_list<ZSubpassDependency> deps)
{
	return createRenderPassImpl(device, nullptr, colorFormats, clearColors, initialColorLayout, finalColorLayout, deps);
}

ZRenderPass	createMultiViewRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
									   std::optional<std::vector<VkClearValue>> clearColors,
									   std::initializer_list<ZSubpassDependency> dependencies,
									   VkImageLayout initialColorLayout, VkImageLayout finalColorLayout)
{
	std::vector<uint32_t>	viewMasks(1);
	std::vector<int32_t>	offsets(dependencies.size());

	uint32_t	selfDependencyCount = 0;
	// There must be at least one subpass
	uint32_t	subpassCount = 1u;
	for (add_cref<ZSubpassDependency> dep : dependencies)
	{
		switch (dep.getType())
		{
		case ZSubpassDependency::Begin:
			viewMasks.at(subpassCount - 1u) = dep.getMultiViewMask();
			break;
		case ZSubpassDependency::Self:
			viewMasks.at(subpassCount - 1u + selfDependencyCount) = dep.getMultiViewMask();
			selfDependencyCount = selfDependencyCount + 1u;
			if (selfDependencyCount > 1u)
			{
				subpassCount = subpassCount + 1u;
				viewMasks.emplace_back(dep.getMultiViewMask());
			}
			break;
		case ZSubpassDependency::Between:
			viewMasks.at(subpassCount - 1u) = dep.getMultiViewMask();
			subpassCount = subpassCount + 1u;
			viewMasks.emplace_back(dep.getMultiViewMask());
			break;
		case ZSubpassDependency::End:
			viewMasks.at(subpassCount - 1u) = dep.getMultiViewMask();
			break;
		}
	}

	VkRenderPassMultiviewCreateInfo info = makeVkStruct();
	info.subpassCount			= subpassCount;
	info.pViewMasks				= viewMasks.data();
	info.dependencyCount		= data_count(dependencies);
	info.pViewOffsets			= offsets.data();
	info.correlationMaskCount	= 0u;
	info.pCorrelationMasks		= nullptr;

	return createRenderPassImpl(device, &info, colorFormats, clearColors, initialColorLayout, finalColorLayout, dependencies);
}

ZRenderPass framebufferGetRenderPass (ZFramebuffer framebuffer)
{
	return framebuffer.getParam<ZRenderPass>();
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

ZDevice getSharedDevice()
{
	//if (getGlobalAppFlags().verbose)
	//{
	//	std::cout << "[INFO] " << __func__ << ' ' << globalSharedDevice << std::endl;
	//}
	//return globalSharedDevice;
	return {};
}

ZPhysicalDevice getSharedPhysicalDevice ()
{
	//ZPhysicalDevice dev = globalSharedDevice.getParam<ZPhysicalDevice>();
	//if (getGlobalAppFlags().verbose)
	//{
	//	std::cout << "[INFO] " << __func__ << ' ' << dev << std::endl;
	//}
	//return dev;
	return {};
}
ZInstance getSharedInstance ()
{
	//ZInstance inst = globalSharedDevice.getParam<ZPhysicalDevice>().getParam<ZInstance>();
	//if (getGlobalAppFlags().verbose)
	//{
	//	std::cout << "[INFO] " << __func__ << ' ' << inst << std::endl;
	//}
	//return inst;
	return {};
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
	Logger				logger				{};
	ZInstance			instance			(VK_NULL_HANDLE, callbacks, apiVersion, {}, {}, logger);
	add_ref<strings>	requiredLayers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	add_ref<strings>	availableExtensions	= instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>();

	requiredLayers = desiredLayers;
	if (enableDebugPrintf)
		requiredLayers.push_back("VK_LAYER_KHRONOS_validation");

	strings availableLayers;
	if (requiredLayers.size())
	{
		availableLayers = enumerateInstanceLayers();
		if (!containsAllStrings(availableLayers, requiredLayers))
			ASSERTMSG(false, "All required layer(s) must match to available instance layer(s)!!!");
	}

	availableExtensions	= enumerateInstanceExtensions(requiredLayers);

	ASSERTMSG(containsAllStrings(availableExtensions, desiredExtensions),
			  "All required extension(s) must match available instance extension(s)");

	VkValidationFeatureEnableEXT						enabledValidationFeature = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
	VkValidationFeaturesEXT								validationFeatures = makeVkStruct();
	validationFeatures.pEnabledValidationFeatures		= &enabledValidationFeature;
	validationFeatures.enabledValidationFeatureCount	= 1;
	validationFeatures.pDisabledValidationFeatures		= nullptr;
	validationFeatures.disabledValidationFeatureCount	= 0;

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
	VkDebugReportCallbackCreateInfoEXT debugReportInfo{};
	const std::string debugUtilsExtName(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		// "VK_EXT_debug_utils"
	const std::string debugReportExtName(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// "VK_EXT_debug_report"
	const bool debugReportEnabled		= ((containsString(debugReportExtName, desiredExtensions) || pReport != nullptr)
										&& containsString(debugReportExtName, availableExtensions));
	const bool debugMessengerEnabled	= ((containsString(debugUtilsExtName, desiredExtensions) || pMessenger != nullptr)
										&& containsString(debugUtilsExtName, availableExtensions)) && (!debugReportEnabled);
	UNREF(pMessengerUserData);
	UNREF(pReportUserData);

	void* pNext = nullptr, **p2pNext = &pNext;

	if (debugMessengerEnabled)
	{
		makeDebugCreateInfo(debugMessengerInfo, &instance.getParamRef<Logger>(), nullptr, enableDebugPrintf);
	}

	if (debugReportEnabled)
	{
		makeDebugCreateInfo(debugReportInfo, &instance.getParamRef<Logger>(), nullptr, enableDebugPrintf);
	}

	if (debugMessengerEnabled || debugReportEnabled)
	{
		*p2pNext = &validationFeatures;
		p2pNext = (void**)&validationFeatures.pNext;
	}

	VkApplicationInfo	appInfo			= makeVkStruct();
	appInfo.pApplicationName			= appName;
	appInfo.applicationVersion			= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName					= nullptr;
	appInfo.engineVersion				= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion					= apiVersion;	// default VK_API_VERSION_1_0

	std::vector<const char*>			v_layerNames(to_cstrings(requiredLayers));
	std::vector<const char*>			v_extensions(to_cstrings(availableExtensions));

	VkInstanceCreateInfo createInfo		= makeVkStruct();
	createInfo.pApplicationInfo			= &appInfo;
	createInfo.enabledExtensionCount	= data_count(v_extensions);
	createInfo.ppEnabledExtensionNames	= data_or_null(v_extensions);
	createInfo.enabledLayerCount		= data_count(v_layerNames);
	createInfo.ppEnabledLayerNames		= data_or_null(v_layerNames);

	VKASSERT3(vkCreateInstance(&createInfo, callbacks, instance.setter()), "Failed to create instance!");

	if (debugMessengerEnabled) createDebugMessenger(instance, callbacks, debugMessengerInfo, *pMessenger);
	if (debugReportEnabled) createDebugReport(instance, callbacks, debugReportInfo, *pReport);

	if (getGlobalAppFlags().verbose)
	{
		Version nullApiVersion = getVulkanImplVersion();
		Version clientApiVersion = Version::fromUint(apiVersion);
		Version vulkanApiVersion = getVulkanImplVersion(instance);
		logger << "[APP] Trying to create versioned instance: " << clientApiVersion << std::endl;
		logger << "[APP] Vulkan Null Instance Version:        " << nullApiVersion << std::endl;
		logger << "[APP] Vulkan Implementation Version:       " << vulkanApiVersion << std::endl;
	}

	instance.verbose(getGlobalAppFlags().verbose != 0);

	return instance;
}

ZPhysicalDevice	getPhysicalDeviceByIndex (ZInstance instance, uint32_t physicalDeviceIndex)
{
	std::vector<VkPhysicalDevice> devices;
	const uint32_t				deviceCount = enumeratePhysicalDevices(*instance, devices);
	ASSERTMSG(physicalDeviceIndex < deviceCount, "Failed to find GPUs with Vulkan support!");
	VkPhysicalDevice			physDevice	= devices.at(physicalDeviceIndex);
	add_cref<strings>			layers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	const strings				extensions	= enumerateDeviceExtensions(devices.at(physicalDeviceIndex), layers);
	VkPhysicalDeviceProperties	deviceProps	{};
	vkGetPhysicalDeviceProperties(physDevice, &deviceProps);
	auto dev = ZPhysicalDevice::create(physDevice, instance.getParam<VkAllocationCallbacksPtr>(), instance,
									   physicalDeviceIndex, extensions, deviceProps);
	dev.verbose(getGlobalAppFlags().verbose != 0);
	return dev;
}

ZPhysicalDevice selectPhysicalDevice(const int								proposedDeviceIndex,
									 ZInstance								instance,
									 add_cref<strings>						requiredExtensions,
									 ZSurfaceKHR							surface)
{	
	std::vector<VkPhysicalDevice> devices;
	const uint32_t deviceCount = enumeratePhysicalDevices(*instance, devices);
	ASSERTMSG(deviceCount != 0, "Failed to find GPUs that suppors Vulkan!");

	uint32_t					physicalDeviceIndex	= INVALID_UINT32;
	add_cref<strings>			layers				= instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	const strings				extensionToRemove	{ VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	strings						availableExtensions;
	VkPhysicalDeviceProperties	deviceProperties;

	auto isDeviceSuitable = [&](VkPhysicalDevice device)
	{
		availableExtensions = enumerateDeviceExtensions(device, layers);
		if (containsString(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, availableExtensions)
			&& containsString(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, availableExtensions))
		{
			removeStrings(extensionToRemove, availableExtensions);
		}
		std::vector<uint32_t> surfaceSupportQueueIndices = findSurfaceSupportedQueueFamilyIndices(device, surface);
		const bool effectiveSurfaceSupport = surface.has_handle() ? (surfaceSupportQueueIndices.size() != 0) : true;
		return (containsAllStrings(availableExtensions, requiredExtensions) && effectiveSurfaceSupport);
	};

	VkPhysicalDevice result = VK_NULL_HANDLE;
	if (proposedDeviceIndex < 0)
	{
		uint32_t i = 0;
		for ( ; i < deviceCount; ++i)
		{
			if (VkPhysicalDevice device = devices.at(i); isDeviceSuitable(device))
			{
				physicalDeviceIndex = i;
				result = device;
				break;
			}
		}
		ASSERTMSG((i < deviceCount), "No GPU found that supports required extension or queues");
	}
	else if (make_unsigned(proposedDeviceIndex) < deviceCount)
	{
		VkPhysicalDevice device = devices[make_unsigned(proposedDeviceIndex)];
		if (isDeviceSuitable(device))
		{
			physicalDeviceIndex = make_unsigned(proposedDeviceIndex);
			result = device;
		}
		else
		{
			std::ostringstream oss;
			oss << "Selected GPU " << proposedDeviceIndex << " does not support desired queues!";
			oss.flush();
			ASSERTMSG(false, oss.str());
		}
	}
	else
	{
		std::ostringstream oss;
		oss << "Selected device index " << proposedDeviceIndex
			<< " exceeds available physical device count " << deviceCount;
		oss.flush();
		ASSERTMSG(false, oss.str());
	}

	vkGetPhysicalDeviceProperties(result, &deviceProperties);

	auto dev = ZPhysicalDevice(result,
						   instance.getParam<VkAllocationCallbacksPtr>(), instance,
						   physicalDeviceIndex, availableExtensions, deviceProperties);
	dev.verbose(getGlobalAppFlags().verbose != 0);

	return dev;
}

ZDevice createLogicalDevice	(ZPhysicalDevice		physDevice,
							 OnEnablingFeatures		onEnablingFeatures,
							 ZSurfaceKHR			surface,
							 bool					enableDebugPrintf)
{	
	uint32_t								queueFamilyPropCount = 0;
	std::vector<float>						queuePriorities;
	std::vector<VkQueueFamilyProperties>	queueFamilyProps;
	std::vector<VkDeviceQueueCreateInfo>	queueCreateInfos;
	std::vector<ZDeviceQueueCreateInfo>		queueCreateExInfos;

	add_ref<Logger>	logger = physDevice.getParam<ZInstance>().getParamRef<Logger>();

	vkGetPhysicalDeviceQueueFamilyProperties(*physDevice, &queueFamilyPropCount, nullptr);
	ASSERTMSG(queueFamilyPropCount, "Unable to get VkQueueFamilyProperties");
	queueFamilyProps.resize(queueFamilyPropCount);
	queueCreateInfos.resize(queueFamilyPropCount);
	queueCreateExInfos.resize(queueFamilyPropCount);
	vkGetPhysicalDeviceQueueFamilyProperties(*physDevice, &queueFamilyPropCount, queueFamilyProps.data());

	auto makeQueueCreateInfo = [&](uint32_t familyIndex, uint32_t queueCount) -> VkDeviceQueueCreateInfo
	{
		VkDeviceQueueCreateInfo qci = makeVkStruct();
		qci.flags				= VkDeviceQueueCreateFlags(0);
		qci.queueFamilyIndex	= familyIndex;
		qci.queueCount			= queueCount;
		qci.pQueuePriorities	= queuePriorities.data();
		return qci;
	};

	if (getGlobalAppFlags().verbose)
	{
		logger << "[INFO} Create device with " << queueFamilyPropCount << " queue family indices" << std::endl;
	}

	{
		uint32_t queuePriorityCount = 0u;
		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyPropCount; ++queueFamilyIndex)
		{
			queuePriorityCount = std::max(queuePriorityCount, queueFamilyProps[queueFamilyIndex].queueCount);
		}
		queuePriorities.resize(queuePriorityCount, 1.0f);
	}

	const std::vector<uint32_t> surfaceSupportedIndices = findSurfaceSupportedQueueFamilyIndices(*physDevice, surface);
	for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyPropCount; ++queueFamilyIndex)
	{
		add_ref<VkDeviceQueueCreateInfo>	queueCreateInfo		= queueCreateInfos[queueFamilyIndex];
		add_ref<ZDeviceQueueCreateInfo>		queueCreateExInfo	= queueCreateExInfos[queueFamilyIndex];
		add_cref<VkQueueFamilyProperties>	queueProperties		= queueFamilyProps[queueFamilyIndex];

		const VkDeviceQueueCreateInfo infoTemplate = makeQueueCreateInfo(queueFamilyIndex, queueProperties.queueCount);
		queueCreateInfo	= infoTemplate;
		static_cast<add_ref<VkDeviceQueueCreateInfo>>(queueCreateExInfo) = infoTemplate;
		queueCreateExInfo.queueFlags = queueProperties.queueFlags;
		queueCreateExInfo.surfaceSupport = surfaceSupportedIndices.end() !=
				std::find(surfaceSupportedIndices.begin(), surfaceSupportedIndices.end(), queueFamilyIndex);
		for (uint32_t availableIndex = 0u; availableIndex < queueProperties.queueCount; ++ availableIndex)
		{
			queueCreateExInfo.queues.set(availableIndex);
		}

		if (getGlobalAppFlags().verbose)
		{
			//::vtf::operator<<(logger, queueCreateExInfo) << std::endl;
			// TODO logger << queueCreateExInfo << std::endl;
		}
	}

	VkPhysicalDeviceFeatures2	features	{};
	ZInstance					instance	= physDevice.getParam<ZInstance>();
	VkAllocationCallbacksPtr	callbacks	= instance.getParam<VkAllocationCallbacksPtr>();
	std::vector<const char*>	layers		(to_cstrings(instance.getParamRef<ZDistType<RequiredLayers,strings>>().get()));

	strings	availableDeviceExtensions(physDevice.getParamRef<strings>());
	removeStrings(getGlobalAppFlags().excludedDevExtensions, availableDeviceExtensions);
	if (onEnablingFeatures)	features	= onEnablingFeatures(physDevice, availableDeviceExtensions);
	features.sType = mkstype<decltype(features)>;
	// Add common features
	{
		if (enableDebugPrintf)
		{
			availableDeviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
		}
		// vertexPipelineStoresAndAtomics & fragmentStoresAndAtomics must be enabled
		VkPhysicalDeviceFeatures tmp{};
		vkGetPhysicalDeviceFeatures(*physDevice, &tmp);
		features.features.vertexPipelineStoresAndAtomics = tmp.vertexPipelineStoresAndAtomics;
		features.features.fragmentStoresAndAtomics = tmp.fragmentStoresAndAtomics;
	}

	std::vector<const char*>	extensions	(to_cstrings(availableDeviceExtensions));

	VkDeviceCreateInfo createInfo		= makeVkStruct();

	createInfo.queueCreateInfoCount		= data_count(queueCreateInfos);
	createInfo.pQueueCreateInfos		= queueCreateInfos.data();

	createInfo.pNext					= onEnablingFeatures ? &features : nullptr;
	createInfo.pEnabledFeatures			= onEnablingFeatures ? nullptr : &features.features;

	createInfo.enabledExtensionCount	= data_count(extensions);
	createInfo.ppEnabledExtensionNames	= data_or_null(extensions);
	createInfo.enabledLayerCount		= data_count(layers);
	createInfo.ppEnabledLayerNames		= data_or_null(layers);

	ZDevice logicalDevice(VK_NULL_HANDLE, callbacks, physDevice, std::move(queueCreateExInfos));
	VKASSERT2(vkCreateDevice(*physDevice, &createInfo, callbacks, logicalDevice.setter()));
	logicalDevice.verbose(getGlobalAppFlags().verbose != 0);

	return logicalDevice;
}

ZPhysicalDevice	deviceGetPhysicalDevice (ZDevice device)
{
	return device.getParam<ZPhysicalDevice>();
}

ZQueue deviceGetNextQueue (ZDevice device, VkQueueFlags queueFlags, bool mustSupportSurface)
{
	UNREF(device);
	UNREF(queueFlags);
	UNREF(mustSupportSurface);
	auto findLSB = [](const auto& bitset) -> uint32_t
	{
		for (std::size_t i = 0; i < bitset.size(); ++i)
			if (bitset.test(i))
				return static_cast<uint32_t>(i);
		return INVALID_UINT32;
	};
	add_ref<std::vector<ZDeviceQueueCreateInfo>> infos =
		device.getParamRef<std::vector<ZDeviceQueueCreateInfo>>();
	for (add_ref<ZDeviceQueueCreateInfo> info : infos)
	{
		const bool allowSupportSurface = mustSupportSurface ? info.surfaceSupport : true;
		if (((queueFlags & info.queueFlags) != 0) && allowSupportSurface)
		{
			const uint32_t queueIndex = findLSB(info.queues);
			if (queueIndex != INVALID_UINT32)
			{
				info.queues.reset(queueIndex);
				VkQueue handle = VK_NULL_HANDLE;
				vkGetDeviceQueue(*device, info.queueFamilyIndex, queueIndex, &handle);
				ZQueue q = ZQueue::create(handle, info.queueFamilyIndex, queueIndex, info.queueFlags, device, info.surfaceSupport);
				q.verbose(getGlobalAppFlags().verbose != 0);
				return q;
			}
		}
	}
	return ZQueue();
}

add_cref<VkPhysicalDeviceProperties> deviceGetPhysicalProperties (ZDevice device)
{
	ASSERTMSG(device.has_handle(), "Device must have handle");
	return device.getParam<ZPhysicalDevice>().getParamRef<VkPhysicalDeviceProperties>();
}

add_cref<VkPhysicalDeviceLimits> deviceGetPhysicalLimits (ZDevice device)
{
	ASSERTMSG(device.has_handle(), "Device must have handle");
	return deviceGetPhysicalLimits(device.getParam<ZPhysicalDevice>());
}

add_cref<VkPhysicalDeviceLimits> deviceGetPhysicalLimits (ZPhysicalDevice device)
{
	ASSERTMSG(device.has_handle(), "Physical Device must have handle");
	return device.getParamRef<VkPhysicalDeviceProperties>().limits;
}

VkPhysicalDeviceFeatures2 deviceGetPhysicalFeatures2 (ZPhysicalDevice device, void* pNext)
{
	VkPhysicalDeviceFeatures2 features = makeVkStruct(pNext);
	vkGetPhysicalDeviceFeatures2(*device, &features);
	return features;
}

uint32_t queueGetFamilyIndex (ZQueue queue)
{
	return queue.has_handle() ? queue.getParamRef<ZDistType<QueueFamilyIndex, uint32_t>>().get() : INVALID_UINT32;
}

uint32_t queueGetIndex (ZQueue queue)
{
	return queue.has_handle() ? queue.getParamRef<ZDistType<QueueIndex, uint32_t>>().get() : INVALID_UINT32;
}

VkQueueFlags queueGetFlags (ZQueue queue)
{
	return queue.has_handle() ? queue.getParamRef<ZDistType<QueueFlags, VkQueueFlags>>().get() : 0;
}

bool queueSupportSwapchain (ZQueue queue)
{
	return queue.has_handle() ? queue.getParamRef<bool>() : false;
}

ZFence createFence (ZDevice device, bool signaled)
{
	VkFence handle = VK_NULL_HANDLE;
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();

	VkFenceCreateInfo fenceInfo = makeVkStruct();
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
	if (auto count = data_count(fences); count)
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
	if (auto count = data_count(fences); count != 0)
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
	if (auto count = data_count(fences); count != 0)
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
	if (auto count = data_count(fences); count != 0)
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

bool fenceStatus (ZFence fence)
{
	const VkResult res = vkGetFenceStatus(*fence.getParamRef<ZDevice>(), *fence);
	const bool status = res == VK_SUCCESS;
	ASSERTION(res != VK_ERROR_DEVICE_LOST);
	return status;
}

ZSemaphore createSemaphore (ZDevice device)
{
	VkSemaphore handle = VK_NULL_HANDLE;
	auto callbacks = device.getParam<VkAllocationCallbacksPtr>();

	VkSemaphoreCreateInfo semInfo = makeVkStruct();
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

	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();
	uint8_t**		pointer	= &memory.getParamRef<uint8_t*>();
	if (vkMapMemory(*device, *memory, 0, size, (VkMemoryMapFlags)0, reinterpret_cast<void**>(pointer)) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to map memory");
	}

	return memory.getParam<uint8_t*>();
}

void unmapMemory (ZDeviceMemory memory)
{
	ZDevice	device	= memory.getParam<ZDevice>();
	vkUnmapMemory(*device, *memory);
	memory.getParamRef<uint8_t*>() = nullptr;
}

void flushMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkMappedMemoryRange	range = makeVkStruct();
	range.memory	= *memory;
	range.offset	= 0u;
	range.size		= size;

	vkFlushMappedMemoryRanges(*device, 1, &range);
}

void invalidateMemory (ZDeviceMemory memory)
{
	ZDevice			device	= memory.getParam<ZDevice>();
	VkDeviceSize	size	= memory.getParam<VkDeviceSize>();

	VkMappedMemoryRange	range = makeVkStruct();
	range.memory	= *memory;
	range.offset	= 0u;
	range.size		= size;

	vkInvalidateMappedMemoryRanges(*device, 1, &range);
}


ZImageView framebufferGetView (ZFramebuffer framebuffer, uint32_t index)
{
	return framebuffer.getParamRef<std::vector<ZImageView>>().at(index);
}

ZImage framebufferGetImage (ZFramebuffer framebuffer, uint32_t index)
{
	return framebufferGetView(framebuffer, index).getParam<ZImage>();
}

VkFormat selectBestDepthStencilFormat (ZPhysicalDevice					device,
									   std::initializer_list<VkFormat>	formats,
									   VkImageTiling					imageTiling,
									   VkFormatFeatureFlags				featuresFlags)
{
	VkFormatProperties	props;
	for (auto i = formats.begin(); i != formats.end(); ++i)
	{
		vkGetPhysicalDeviceFormatProperties(*device, *i, &props);

		VkFormatFeatureFlags features = 0;
		switch (imageTiling)
		{
			case VK_IMAGE_TILING_LINEAR:
				features = props.linearTilingFeatures;
				break;
			case VK_IMAGE_TILING_OPTIMAL:
				features = props.optimalTilingFeatures;
				break;
			default: ASSERTION(0);
		}

		if ((features & featuresFlags) == featuresFlags)
			return *i;
	}
	ASSERTION(0);
	return VK_FORMAT_UNDEFINED;
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
