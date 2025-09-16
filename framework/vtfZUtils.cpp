#include "vtfVkUtils.hpp"
#include "vtfCUtils.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfFormatUtils.hpp"
#include "vtfStructUtils.hpp"
#include "vtfZImage.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZBuffer.hpp"
#include "vtfZDeviceMemory.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"
#include "vtfThreadSafeLogger.hpp"
#include "vtfProgressRecorder.hpp"
#include "vtfVulkanDriver.hpp"

void releaseQueue (const QueueParams& params)
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
	return str << "(" << v.nmajor << ", " << v.nminor << ", " << v.npatch << ")";
}

std::ostream& operator<<(std::ostream& str, const VtfVersion& v)
{
	return str	<< v.get().nmajor << '.'
				<< v.get().nminor << '.'
				<< v.get().npatch << '.'
				<< v.get().nvariant;
}

ZShaderModule	createShaderModule (ZDevice device, VkShaderStageFlagBits stage,
                                    add_cref<std::vector<char>> base64Code, add_cref<std::string> entryName)
{
	const std::vector<uint8_t> code = base64_decode(base64Code);
	const auto size = code.size();
	ASSERTMSG(size != 0u && size % 4u == 0u, "Base64 code size (", size, ") must be four bytes multiplication");
	return createShaderModule(device, stage,
			reinterpret_cast<add_cptr<uint32_t>>(code.data()), size_t(size), entryName);
}

ZShaderModule createShaderModule (
    ZDevice					device,
    VkShaderStageFlagBits	stage,
    add_cptr<uint32_t>		pCode,
	size_t					codeSize,
    add_cref<std::string>	entryName)
{
	ASSERTMSG(reinterpret_cast<uintptr_t>(pCode) % alignof(uint32_t) == 0, "Shader byte buffer is not 4-byte aligned");

	VkShaderModuleCreateInfo createInfo = makeVkStruct();
	createInfo.codeSize = codeSize;
	createInfo.pCode	= pCode;
	// Magic number: 0x07230203

	VkShaderModule				shaderModule	= VK_NULL_HANDLE;
	VkAllocationCallbacksPtr	callbacks		= device.getParam<VkAllocationCallbacksPtr>();

    std::vector<uint32_t> code2;

	VKASSERT(vkCreateShaderModule(*device, &createInfo, callbacks, &shaderModule));

	return ZShaderModule::create(shaderModule, device, callbacks, stage, entryName);
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
	ASSERTMSG((renderAttachmentCount <= attachmentCount),
		"RenderPass attachments (", renderAttachmentCount, ") must not exceed "
		"framebuffer's attachment count (", attachmentCount, ") being created");
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
	framebufferInfo.attachmentCount	= renderAttachmentCount;
	framebufferInfo.pAttachments	= views.data();
	framebufferInfo.width			= width;
	framebufferInfo.height			= height;
	framebufferInfo.layers			= 1;

	VKASSERT(vkCreateFramebuffer(*device, &framebufferInfo, allocationCallbacks, &framebuffer));

	return ZFramebuffer::create(framebuffer, device, allocationCallbacks, width, height, renderPass, attachments);
}

ZRenderPassBeginInfo::ZRenderPassBeginInfo (ZCommandBuffer cmd, ZFramebuffer framebuffer,
											uint32_t subpass, VkSubpassContents contents)
	: ZRenderPassBeginInfo(cmd, framebuffer.getParam<ZRenderPass>(), framebuffer, subpass, contents)
{
}
ZRenderPassBeginInfo::ZRenderPassBeginInfo (ZCommandBuffer cmd, ZRenderPass renderPass, ZFramebuffer framebuffer,
											uint32_t subpass, VkSubpassContents contents)
	: m_cmdBuffer			(cmd)
	, m_framebuffer			(framebuffer)
	, m_renderPass			(renderPass)
	, m_subpass				(subpass)
	, m_contents			(contents)
{
}
VkRenderPassBeginInfo ZRenderPassBeginInfo::operator ()() const
{
	const uint32_t width = m_framebuffer.getParam<ZDistType<Width, uint32_t>>();
	const uint32_t height = m_framebuffer.getParam<ZDistType<Height, uint32_t>>();
	add_cref<std::vector<VkClearValue>> clearValues = m_renderPass.getParamRef<std::vector<VkClearValue>>();

	VkRenderPassBeginInfo	info = makeVkStruct();
	info.renderPass					= *m_renderPass;
	info.framebuffer				= *m_framebuffer;
	info.renderArea.offset.x		= 0u;
	info.renderArea.offset.y		= 0u;
	info.renderArea.extent.width	= width;
	info.renderArea.extent.height	= height;
	info.clearValueCount			= data_count(clearValues);
	info.pClearValues				= data_or_null(clearValues);

	return info;
}

static ZRenderPass	createRenderPassImpl
(
	ZDevice device,
	void* pNext, 
	VkFormat depthStencilFormat,
	add_cref<std::vector<VkFormat>> colorFormats,
	add_cref<std::vector<VkClearValue>> clearColors,
	VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
	add_cref<std::vector<ZSubpassDependency>> deps
)
{
	const uint32_t attachmentCount = data_count(colorFormats);
	ASSERTMSG(colorFormats.size(), "pColorAttachments must contain at least one element");
	ASSERTMSG(deviceGetPhysicalLimits(device).maxColorAttachments > attachmentCount,
			  "Attachments you want exceed maxAttachmens");

	add_cptr<std::vector<VkClearValue>> pClearColors = clearColors.size() ? &clearColors : nullptr;
	const uint32_t clearColorCount = data_count(clearColors);

	VkAttachmentDescription colorDescTemplate{};
	colorDescTemplate.flags				= VkAttachmentDescriptionFlags(0);
	colorDescTemplate.format			= VK_FORMAT_UNDEFINED;
	colorDescTemplate.samples			= VK_SAMPLE_COUNT_1_BIT;
	colorDescTemplate.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorDescTemplate.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	colorDescTemplate.stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorDescTemplate.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorDescTemplate.initialLayout		= initialColorLayout;
	colorDescTemplate.finalLayout		= finalColorLayout;

	bool enableDepthStencil = false;
	VkAttachmentDescription depthStencilDescTemplate{};
	VkAttachmentReference	depthStencilRefTemplate{};
	if (VK_FORMAT_UNDEFINED != depthStencilFormat)
	{
		const std::pair<bool, bool> isDS = formatIsDepthStencil(device.getParam<ZPhysicalDevice>(), depthStencilFormat, true);
		if (false == isDS.first)
		{
			ASSERTFALSE("depthStencilFormat is not Depth or Stencil format or is not supported by device");
		}
		enableDepthStencil = true;
		depthStencilDescTemplate.flags			= VkAttachmentDescriptionFlags(0);
		depthStencilDescTemplate.format			= depthStencilFormat;
		depthStencilDescTemplate.samples		= VK_SAMPLE_COUNT_1_BIT;
		depthStencilDescTemplate.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthStencilDescTemplate.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
		depthStencilDescTemplate.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthStencilDescTemplate.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthStencilDescTemplate.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
		depthStencilDescTemplate.finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthStencilRefTemplate.attachment = attachmentCount;
		depthStencilRefTemplate.layout = depthStencilDescTemplate.finalLayout;
	}

	VkAttachmentReference	colorRefTemplate{};
	colorRefTemplate.attachment	= 0u;
	colorRefTemplate.layout		= finalColorLayout;

	std::vector<VkAttachmentDescription>	descriptions(attachmentCount, colorDescTemplate);
	std::vector<VkAttachmentReference>		references(attachmentCount, colorRefTemplate);

	for (uint32_t attachment = 0; attachment < attachmentCount; ++attachment)
	{
		ASSERTMSG(colorFormats[attachment] != VK_FORMAT_UNDEFINED, "Format must not be VK_FORMAT_UNDEFINED");
		add_ref<VkAttachmentDescription> desc = descriptions.at(attachment);
		desc.format	= colorFormats.at(attachment);
		if (attachment < clearColorCount)
		{
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}
		else if (initialColorLayout == VK_IMAGE_LAYOUT_MAX_ENUM)
		{
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			desc.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		}
		else
		{
			desc.loadOp =  VK_ATTACHMENT_LOAD_OP_LOAD;
		}
		references.at(attachment).attachment = attachment;
	}

	if (enableDepthStencil)
	{
		descriptions.emplace_back(depthStencilDescTemplate);
	}

	VkSubpassDescription subpassTemplate;
	subpassTemplate.flags					= VkSubpassDescriptionFlags(0);
	subpassTemplate.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassTemplate.colorAttachmentCount	= data_count(references);
	subpassTemplate.pColorAttachments		= data_or_null(references);
	subpassTemplate.pDepthStencilAttachment = enableDepthStencil ? &depthStencilRefTemplate : nullptr;
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
	renderPassInfo.attachmentCount	= data_count(descriptions);
	renderPassInfo.pAttachments		= data_or_null(descriptions);
	renderPassInfo.subpassCount		= data_count(subpasses);
	renderPassInfo.pSubpasses		= data_or_null(subpasses);
	renderPassInfo.dependencyCount	= data_count(subpassDeps);
	renderPassInfo.pDependencies	= data_or_null(subpassDeps);

	const VkAllocationCallbacksPtr	callbacks	= device.getParam<VkAllocationCallbacksPtr>();
	ZRenderPass	renderPass = ZRenderPass::create(VK_NULL_HANDLE, device, callbacks,
												 attachmentCount, renderPassInfo.subpassCount,
												 {/*clearValues*/}, depthStencilFormat, finalColorLayout, {}, {});
	VKASSERT(vkCreateRenderPass(*device, &renderPassInfo, callbacks, renderPass.setter()));

	add_ref<std::vector<VkClearValue>> clearValues = renderPass.getParamRef<std::vector<VkClearValue>>();
	clearValues.resize(renderPassInfo.attachmentCount);
	for (uint32_t a = 0u; a < renderPassInfo.attachmentCount; ++a)
	{
		if (pClearColors && (a < pClearColors->size()))
			clearValues[a] = pClearColors->at(a);
	}
	if (enableDepthStencil)
	{
		clearValues.back().depthStencil.depth = 1.0f;
	}

	return renderPass;
}

ZRenderPass	createColorRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
								   std::vector<VkClearValue> clearColors,
								   VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
								   std::initializer_list<ZSubpassDependency> deps)
{
	return createRenderPassImpl(device, nullptr, VK_FORMAT_UNDEFINED, colorFormats, clearColors, initialColorLayout, finalColorLayout, deps);
}

ZRenderPass	createColorRenderPass (ZDevice device, VkFormat depthStencilFormat,
								   add_cref<std::vector<VkFormat>> colorFormats,
								   std::vector<VkClearValue> clearColors,
								   VkImageLayout initialColorLayout, VkImageLayout finalColorLayout,
								   std::initializer_list<ZSubpassDependency> deps)
{
	return createRenderPassImpl(device, nullptr, depthStencilFormat, colorFormats, clearColors, initialColorLayout, finalColorLayout, deps);
}

ZRenderPass	createMultiViewRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
									   std::vector<VkClearValue> clearColors,
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

	return createRenderPassImpl(device, &info, VK_FORMAT_UNDEFINED, colorFormats, clearColors, initialColorLayout, finalColorLayout, dependencies);
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

strings upgradeInstanceExtensions (add_cref<strings> desiredExtensions)
{
	strings e = desiredExtensions;
	const strings exts = enumerateInstanceExtensions();
	auto addExt = [&](std::string ext) -> void
	{
		if (containsString(exts, ext))
			e.emplace_back(std::move(ext));
	};

	addExt("VK_KHR_surface");
#if SYSTEM_OS_WINDOWS == 1
	addExt("VK_KHR_win32_surface");
#elif SYSTEM_OS_LINUX == 1
	addExt("VK_KHR_xcb_surface");
	addExt("VK_KHR_xlib_surface");
#endif
	return e;
}

ZInstance createInstance (
	const char* appName,
	VkAllocationCallbacksPtr	callbacks,
	const strings& desiredLayers,
	const strings& desiredExtensions,
	uint32_t					apiVersion,
	bool						/*enableDebugPrintf 2025-01-12*/)
{
	Logger						logger{};
	ZInstance					instance(VkInstance(VK_NULL_HANDLE)
		, callbacks
		, (appName ? appName : "VTF")
		, apiVersion
		, {/*AvailableLayers*/ }
		, {/*RequiredLayers*/ }
		, {/*AvailableLayerExtensions*/ }
		, {/*RequiredLayerExtensions*/ }
		, logger
		, ProgressRecorder()
		, VkDebugUtilsMessengerEXT(VK_NULL_HANDLE)
		, VkDebugReportCallbackEXT(VK_NULL_HANDLE));
	add_ref<strings>			requiredLayers = instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	add_ref<strings>			availableLayers = instance.getParamRef<ZDistType<AvailableLayers, strings>>();
	add_ref<strings>			availableExtensions = instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>();
	add_ref<strings>			requiredExtensions = instance.getParamRef<ZDistType<RequiredLayerExtensions, strings>>();
	add_ref<ProgressRecorder>	progressRecorder = instance.getParamRef<ProgressRecorder>();
	add_cref<GlobalAppFlags>	gf = getGlobalAppFlags();

	// setup layers
	{
		requiredLayers = distinctStrings(desiredLayers);
		availableLayers = enumerateInstanceLayers();
		ASSERTMSG(containsAllStrings(availableLayers, requiredLayers),
			"All required layer(s) must match to available instance layer(s)!!!");
	}

	const bool validationRequired = !!requiredLayers.size();
	const std::string utilsExt(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		// "VK_EXT_debug_utils"
	const std::string reportExt(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// "VK_EXT_debug_report"

	bool utilsRequired = false;
	bool reportRequired = false;
	bool portabilityDesired = true;
	bool portabilityRequired = false;

	// setup extensions
	{
		availableExtensions = enumerateInstanceExtensions();

		if (containsString(availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
		{
			requiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}

		if (portabilityDesired)
		{
			const std::string portabilityExt(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			if (containsString(availableExtensions, portabilityExt))
			{
				requiredExtensions.push_back(portabilityExt);
				portabilityRequired = true;
			}
		}

		const bool utilsDesired = containsString(utilsExt, desiredExtensions);
		const bool reportDesired = containsString(reportExt, desiredExtensions);
		const bool utilsAvailable = containsString(utilsExt, availableExtensions);
		const bool reportAvailable = containsString(reportExt, availableExtensions);

		// Add the desired extensions, ignoring UTILS and REPORT for now
		for (add_cref<std::string> ext : desiredExtensions) {
			if (ext != utilsExt && ext != reportExt) {
				requiredExtensions.push_back(ext);
			}
		}

		// Prefer UTILS if both are available, but only add one of them
		auto checkDebugExtensions = [&](bool preferReport, bool checkDesirability) -> void
		{
			if (preferReport)
			{
				if (reportAvailable && (checkDesirability ? reportDesired : true)) {
					reportRequired = true;
					requiredExtensions.push_back(reportExt);
				}
				else if (utilsAvailable && (checkDesirability ? utilsDesired : true)) {
					utilsRequired = true;
					requiredExtensions.push_back(utilsExt);
				}
			}
			else
			{
				if (utilsAvailable && (checkDesirability ? utilsDesired : true)) {
					utilsRequired = true;
					requiredExtensions.push_back(utilsExt);
				}
				else if (reportAvailable && (checkDesirability ? reportDesired : true)) {
					reportRequired = true;
					requiredExtensions.push_back(reportExt);
				}
			}
			if ((false == utilsRequired) && (false == reportRequired)) {
				ASSERTMSG(checkDesirability, "???");
			}
		};

		checkDebugExtensions(true, true);

		// If validation is enabled, make sure at least one extension is added
		if (validationRequired && (utilsRequired || reportRequired))
		{
			checkDebugExtensions(true, false);
		}

		distinctStrings(requiredExtensions);

		ASSERTMSG(containsAllStrings(availableExtensions, requiredExtensions),
			"All required extension(s) must match available instance extension(s)");
	}

	/*
	const VkValidationFeatureEnableEXT	enabledValidationFeature[]{
											VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	VkValidationFeaturesEXT				validationFeatures = makeVkStruct();
	validationFeatures.pEnabledValidationFeatures		= enabledValidationFeature;
	validationFeatures.enabledValidationFeatureCount	= data_count(enabledValidationFeature);
	validationFeatures.pDisabledValidationFeatures		= nullptr;
	validationFeatures.disabledValidationFeatureCount	= 0u;
	*/

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
	VkDebugReportCallbackCreateInfoEXT debugReportInfo{};
	void* instanceCreateInfoPnext = nullptr;

	if (utilsRequired)
	{
		ASSERTION(false == reportRequired);
		makeDebugCreateInfo(debugMessengerInfo, &instance.getParamRef<Logger>(), nullptr, false);
	}
	if (reportRequired)
	{
		ASSERTION(false == utilsRequired);
		makeDebugCreateInfo(debugReportInfo, &instance.getParamRef<Logger>(), nullptr, false);
	}

	VkApplicationInfo	appInfo			= makeVkStruct();
	appInfo.pApplicationName			= instance.getParamRef<std::string>().c_str();
	appInfo.applicationVersion			= VK_MAKE_VERSION(gf.vtfVer.nmajor, gf.vtfVer.nminor, gf.vtfVer.npatch);
	appInfo.pEngineName					= "VTF";
	appInfo.engineVersion				= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion					= apiVersion;	// default VK_API_VERSION_1_0

	std::vector<const char*>			v_layerNames(to_cstrings(requiredLayers));
	std::vector<const char*>			v_extensions(to_cstrings(requiredExtensions));

	VkInstanceCreateFlags				flags(VkInstanceCreateFlags(0));
	if (portabilityRequired)
	{
		flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}

	VkInstanceCreateInfo createInfo		= makeVkStruct(instanceCreateInfoPnext);
	createInfo.flags					= flags;
	createInfo.pApplicationInfo			= &appInfo;
	createInfo.enabledExtensionCount	= data_count(v_extensions);
	createInfo.ppEnabledExtensionNames	= data_or_null(v_extensions);
	createInfo.enabledLayerCount		= data_count(v_layerNames);
	createInfo.ppEnabledLayerNames		= data_or_null(v_layerNames);

	if (gf.verbose)
	{
		struct VP
		{
			const Version ver;
			VP(add_cref<Version> v) : ver(v) {}
			std::string operator()() {
				std::ostringstream os;
				os << "(variant=" << ver.nvariant;
				os << ", major=" << ver.nmajor;
				os << ", minor=" << ver.nminor;
				os << ", patch=" << ver.npatch << ')';
				return os.str();
			}
		};

		std::cout << "[APP] Trying to create Vulkan instance:\n"
				  << "       pApplicationName:      " << std::quoted(appInfo.pApplicationName) << std::endl
				  << "       applicationVersion:    " << gf.vtfVer << std::endl
				  << "       pEngineName:           " << std::quoted(appInfo.pEngineName) << std::endl
				  << "       engineVersion:         " << Version::fromUint(appInfo.engineVersion) << std::endl
				  << "       apiVersion:            " << VP(Version::fromUint(apiVersion))() << std::endl;
		std::cout << "       enabledLayerCount:     " << createInfo.enabledLayerCount << std::endl;
		for (uint32_t i = 0; i < createInfo.enabledLayerCount; ++i)
		{
			std::cout << "         " << i << ": " << createInfo.ppEnabledLayerNames[i] << std::endl;
		}
		std::cout << "       enabledExtensionCount: " << createInfo.enabledExtensionCount << std::endl;
		for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
		{
			std::cout << "         " << i << ": " << createInfo.ppEnabledExtensionNames[i] << std::endl;
		}
	}
	auto pfnCreateInstance = getDriverCreateInstanceProc();
	ASSERTMSG(pfnCreateInstance, "vkCreateInstance() must not be null");
	progressRecorder.stamp("Before vkCreateInstance()");
	VKASSERTMSG((*pfnCreateInstance)(&createInfo, callbacks, instance.setter()), "Failed to create instance!");
	progressRecorder.stamp("After vkCreateInstance()");

	if (getGlobalAppFlags().verbose)
	{
		bool driverLoadedStatus = false;
		std::string driverFileName = getDriverFileName(driverLoadedStatus);
		UNREF(driverLoadedStatus);

		Version nullApiVersion = getVulkanImplVersion();
		Version vulkanApiVersion = getVulkanImplVersion(instance);
		logger << "[APP] Vulkan driver file name:       " << driverFileName << std::endl;
		logger << "[APP] Vulkan Null Instance Version:  " << nullApiVersion << std::endl;
		logger << "[APP] Vulkan Implementation Version: " << vulkanApiVersion << std::endl;
	}

	struct AInstance : public ZInstance {
		AInstance(ZInstance i) : ZInstance(i) {}
		void initInterface() {
			ZInstanceSingleton::initInterface(this->operator*());
		}
	};
	AInstance(instance).initInterface();

	if (utilsRequired)
	{
		createDebugMessenger(instance, callbacks, debugMessengerInfo, instance.getParamRef<VkDebugUtilsMessengerEXT>());
	}
	if (reportRequired)
	{
		createDebugReport(instance, callbacks, debugReportInfo, instance.getParamRef<VkDebugReportCallbackEXT>());
	}

	instance.verbose(getGlobalAppFlags().verbose != 0);

	return instance;
}

ZPhysicalDevice	getPhysicalDeviceByIndex (ZInstance instance, uint32_t physicalDeviceIndex)
{
	std::vector<VkPhysicalDevice> devices;
	const uint32_t				deviceCount = enumeratePhysicalDevices(instance, devices);
	ASSERTMSG(physicalDeviceIndex < deviceCount, "Failed to find GPUs with Vulkan support!");
	VkPhysicalDevice			physDevice	= devices.at(physicalDeviceIndex);
	add_cref<strings>			layers		= instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	const strings				extensions	= enumerateDeviceExtensions(devices.at(physicalDeviceIndex), layers);
	VkPhysicalDeviceProperties	deviceProps	{};
	vkGetPhysicalDeviceProperties(physDevice, &deviceProps);
	auto dev = ZPhysicalDevice::create(physDevice, instance.getParam<VkAllocationCallbacksPtr>(), instance,
		physicalDeviceIndex, {/* TODO */}, extensions, deviceProps);
	dev.verbose(getGlobalAppFlags().verbose != 0);
	return dev;
}

ZPhysicalDevice selectPhysicalDevice (
    const int			proposedDeviceIndex,
    ZInstance			instance,
    add_cref<strings>   desiredExtensions,
    ZSurfaceKHR			surface)
{	
	std::vector<VkPhysicalDevice> devices;
	const uint32_t deviceCount = enumeratePhysicalDevices(instance, devices);
	add_cref<ZInstanceInterface> instanceInterface = instance.getInterface();
	ASSERTMSG(deviceCount != 0, "Failed to find GPUs that suppors Vulkan!");

    if (getGlobalAppFlags().verbose)
    {
        std::cout << "[INFO] Found " << deviceCount << " available device(s)" << std::endl;
        for (uint32_t i = 0; i < deviceCount; ++i)
        {
            printPhysicalDevice(instance, devices.at(i), std::cout, i);
        }
    }

	uint32_t					physicalDeviceIndex	= INVALID_UINT32;
	add_cref<strings>			layers				= instance.getParamRef<ZDistType<RequiredLayers, strings>>();
	const strings				extensionToRemove	{ VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	strings						availableExtensions;
	VkPhysicalDeviceProperties	deviceProperties;

	auto isDeviceSuitable = [&](VkPhysicalDevice device)
	{
		availableExtensions = enumerateDeviceExtensions(instance, device, layers);
		// TODO why?
		//if (containsString(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, availableExtensions)
		//	&& containsString(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, availableExtensions))
		//{
		//	removeStrings(extensionToRemove, availableExtensions);
		//}
		std::vector<uint32_t> surfaceSupportQueueIndices = findSurfaceSupportedQueueFamilyIndices(device, surface);
		const bool effectiveSurfaceSupport = surface.has_handle() ? (surfaceSupportQueueIndices.size() != 0) : true;
		return (containsAllStrings(availableExtensions, desiredExtensions) && effectiveSurfaceSupport);
	};

    if (getGlobalAppFlags().verbose)
    {
        std::cout << "[INFO] Trying to select device with index "
                  << proposedDeviceIndex
                  << ", surface needed: " << boolean(surface.has_handle(), true)
                  << std::endl;
    }

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
		ASSERTMSG((i < deviceCount), "No GPU found that supports desired extension or queues");
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
			ASSERTFALSE("Selected GPU ", proposedDeviceIndex, " does not support desired queues!");
		}
	}
	else
	{
		ASSERTFALSE("Selected device index ", proposedDeviceIndex,
			" exceeds available physical device count ", deviceCount);
	}

	instanceInterface.vkGetPhysicalDeviceProperties(result, &deviceProperties);

    if (getGlobalAppFlags().verbose)
    {
        std::cout << "[INFO] Successfully selected device with index :"
                  << physicalDeviceIndex << std::endl;
        printPhysicalDevice(deviceProperties, std::cout, physicalDeviceIndex);
    }

	auto dev = ZPhysicalDevice(result
						, instance.getParam<VkAllocationCallbacksPtr>()
						, instance
						, physicalDeviceIndex
						, std::move(availableExtensions)
						, desiredExtensions
						, deviceProperties);
	dev.verbose(getGlobalAppFlags().verbose != 0);

	return dev;
}

strings upgradeDeviceExtensions (add_cref<strings> desiredExtensions)
{
	strings e = desiredExtensions;
	e.push_back("VK_KHR_swapchain");
	return e;
}

ZDevice createLogicalDevice	(
	ZPhysicalDevice		physDevice,
	OnEnablingFeatures	onEnablingFeatures,
	ZSurfaceKHR			surface,
	bool				/* enableDebugPrintf 2025-01-12 */)
{	
	uint32_t								queueFamilyPropCount = 0;
	std::vector<float>						queuePriorities;
	std::vector<VkQueueFamilyProperties>	queueFamilyProps;
	std::vector<VkDeviceQueueCreateInfo>	queueCreateInfos;
	std::vector<ZDeviceQueueCreateInfo>		queueCreateExInfos;

	add_ref<Logger>	logger = physDevice.getParam<ZInstance>().getParamRef<Logger>();
	add_cref<ZInstanceInterface> ii = physDevice.getParamRef<ZInstance>().getInterface();

	ii.vkGetPhysicalDeviceQueueFamilyProperties(*physDevice, &queueFamilyPropCount, nullptr);
	ASSERTMSG(queueFamilyPropCount, "Unable to get VkQueueFamilyProperties");
	queueFamilyProps.resize(queueFamilyPropCount);
	queueCreateInfos.resize(queueFamilyPropCount);
	queueCreateExInfos.resize(queueFamilyPropCount);
	ii.vkGetPhysicalDeviceQueueFamilyProperties(*physDevice, &queueFamilyPropCount, queueFamilyProps.data());

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
            ::vtf::operator<< (std::cout, queueCreateExInfo);
		}
	}

	ZInstance					instance	= physDevice.getParam<ZInstance>();
	VkAllocationCallbacksPtr	callbacks	= instance.getParam<VkAllocationCallbacksPtr>();
	add_ref<ProgressRecorder>	recorder	= instance.getParamRef<ProgressRecorder>();
	std::vector<const char*>	layers		(to_cstrings(instance.getParamRef<ZDistType<RequiredLayers,strings>>().get()));

	add_cref<strings>	availableExtensions(physDevice.getParamRef<ZDistType<AvailableDeviceExtensions, strings>>().get());
	add_cref<strings>	desiredExtensions(physDevice.getParamRef<ZDistType<DesiredRequiredDeviceExtensions, strings>>().get());

	DeviceCaps deviceCaps(availableExtensions, physDevice);

	add_ref<strings> requiredExtensions(deviceCaps.requiredExtension);
	removeStrings(getGlobalAppFlags().excludedDevExtensions, requiredExtensions);
	mergeStringsDistinct(requiredExtensions, desiredExtensions);

	if (onEnablingFeatures)
	{
		onEnablingFeatures(std::ref(deviceCaps));
	}
	deviceCaps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::fragmentStoresAndAtomics);
	deviceCaps.addUpdateFeatureIf(&VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics);


	//if (enableDebugPrintf)
	//{
	//	availableDeviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	//}

	std::vector<const char*>	extensions	(to_cstrings(requiredExtensions));

	VkDeviceCreateInfo createInfo		= makeVkStruct();

	createInfo.queueCreateInfoCount		= data_count(queueCreateInfos);
	createInfo.pQueueCreateInfos		= queueCreateInfos.data();

	//createInfo.pNext					= established in updateDeviceCreateInfo
	//createInfo.pEnabledFeatures		= established in updateDeviceCreateInfo

	createInfo.enabledExtensionCount	= data_count(extensions);
	createInfo.ppEnabledExtensionNames	= data_or_null(extensions);
	createInfo.enabledLayerCount		= data_count(layers);
	createInfo.ppEnabledLayerNames		= data_or_null(layers);

	DeviceCaps::Features deviceCapsFeatures(deviceCaps.updateDeviceCreateInfo(createInfo));

	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[APP] Trying to create logical device" << std::endl;
		std::cout << "      enabledLayerCount:     " << createInfo.enabledLayerCount << std::endl;
		for (uint32_t i = 0; i < createInfo.enabledLayerCount; ++i)
		{
			std::cout << "        " << i << ": " << createInfo.ppEnabledLayerNames[i] << std::endl;
		}
		std::cout << "      enabledExtensionCount: " << createInfo.enabledExtensionCount << std::endl;
		for (uint32_t i = 0; i < createInfo.enabledExtensionCount; ++i)
		{
			std::cout << "        " << i << ": " << createInfo.ppEnabledExtensionNames[i] << std::endl;
		}
		if (createInfo.pEnabledFeatures)
		{
			std::cout << "      pEnabledFeatures: VkPhysicalDeviceFeatures" << std::endl;
			printPhysicalDeviceFeatures(*createInfo.pEnabledFeatures, std::cout, 10);
		}
		else
		{
			std::cout << "      pEnabledFeatures: nullptr" << std::endl;
		}
		if (createInfo.pNext)
		{
			int indent = 0;
			add_cptr<void> pNext = createInfo.pNext;
			std::cout << "      pNext: ";
			while (pNext) {
				add_cptr<VkBaseOutStructure> base = static_cast<add_cptr<VkBaseOutStructure>>(pNext);
				if (indent)
					std::cout << std::string(13, ' ');
				std::cout << vk::to_string(static_cast<vk::StructureType>(base->sType)) << std::endl;
				if (base->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
				{
					add_cptr<VkPhysicalDeviceFeatures2> f20 = static_cast<add_cptr<VkPhysicalDeviceFeatures2>>(pNext);
					add_cref<VkPhysicalDeviceFeatures> f10 = f20->features;
					printPhysicalDeviceFeatures(f10, std::cout, 15);
				}
				
				pNext = base->pNext;
				indent = 1;
			}
		}
		else
		{
			std::cout << "      pNext: nullptr" << std::endl;
		}
	}

	VkDevice deviceHandle(VK_NULL_HANDLE);
	auto pfnDriverCreateDevice = getDriverCreateDeviceProc();
    ASSERTMSG(pfnDriverCreateDevice, "vkCreateDevice must not be null");
	auto pfnInstanceCreateDevice = getInstanceCreateDeviceProc(*instance);
	ASSERTMSG(pfnInstanceCreateDevice, "vkCreateDevice must not be null");
	recorder.stamp("Before vkCreateDevice()");
	const VkResult createResult = pfnInstanceCreateDevice(*physDevice, &createInfo, callbacks, &deviceHandle);
	recorder.stamp("After vkCreateDevice()");
	VKASSERTMSG(createResult, "Failed to create logical device");
	ZDevice logicalDevice(deviceHandle, callbacks, physDevice, std::move(queueCreateExInfos));
	logicalDevice.verbose(getGlobalAppFlags().verbose != 0);

	struct AInstance : public ZInstance	{
		struct ADevice : public ZDevice	{
			ADevice(ZDevice dev) : ZDevice(dev) {}
			void initInterface(VkInstance instance, VkDevice device) {
				ZDeviceSingleton::initInterface(instance, device);
			}
		};
		AInstance(ZInstance instance) : ZInstance(instance) {}
		void initInterface(ZDevice dev) {
			ADevice(dev).initInterface(this->operator*(), *dev);
		}
	};
	AInstance(instance).initInterface(logicalDevice);

	return logicalDevice;
}

ZPhysicalDevice	deviceGetPhysicalDevice (ZDevice device)
{
	return device.getParam<ZPhysicalDevice>();
}

ZInstance deviceGetInstance(ZPhysicalDevice device)
{
	return device.getParam<ZInstance>();
}

ZInstance deviceGetInstance(ZDevice device)
{
	return deviceGetInstance(deviceGetPhysicalDevice(device));
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

add_cref<VkPhysicalDeviceProperties> deviceGetPhysicalProperties (add_cref<ZDevice> device)
{
	return deviceGetPhysicalProperties(device.getParamRef<ZPhysicalDevice>());
}

add_cref<VkPhysicalDeviceProperties> deviceGetPhysicalProperties (add_cref<ZPhysicalDevice> device)
{
	return device.getParamRef<VkPhysicalDeviceProperties>();
}

VkPhysicalDeviceProperties deviceGetPhysicalProperties2 (add_cref<ZPhysicalDevice> device, add_ptr<void> pNext)
{
	ASSERTMSG(device.has_handle(), "Device must have handle");
	VkPhysicalDeviceProperties2 props = makeVkStruct(pNext);
	auto fn = deviceGetInstance(device).getInterface().vkGetPhysicalDeviceProperties2KHR;
	if (fn)
		fn(*device, &props);
	else vkGetPhysicalDeviceProperties2(*device, &props);
	return props.properties;
}

add_cref<VkPhysicalDeviceLimits> deviceGetPhysicalLimits (add_cref<ZDevice> device)
{
	ASSERTMSG(device.has_handle(), "Device must have handle");
	return deviceGetPhysicalLimits(device.getParam<ZPhysicalDevice>());
}

add_cref<VkPhysicalDeviceLimits> deviceGetPhysicalLimits (add_cref<ZPhysicalDevice> device)
{
	ASSERTMSG(device.has_handle(), "Physical Device must have handle");
	return device.getParamRef<VkPhysicalDeviceProperties>().limits;
}

VkPhysicalDeviceFeatures2 deviceGetPhysicalFeatures2 (ZPhysicalDevice device, void* pNext)
{
	add_cref<ZInstanceInterface> instanceInterface = device.getParamRef<ZInstance>().getInterface();
	VkPhysicalDeviceFeatures2 features = makeVkStruct(pNext);
	instanceInterface.vkGetPhysicalDeviceFeatures2(*device, &features);
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

	VKASSERT(vkCreateFence(*device, &fenceInfo, callbacks, &handle));
	return ZFence::create(handle, device, callbacks);
}

VkResult waitForFence (ZFence fence, uint64_t timeout, bool assertOnFail)
{
	VkDevice device = *fence.getParam<ZDevice>();
	const VkResult res = vkWaitForFences(device, 1, fence.ptr(), VK_TRUE, timeout);
	if (assertOnFail) VKASSERT(res);
	return res;
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
		VKASSERT(vkWaitForFences(*device, count, handles.data(), VK_TRUE, timeout));
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
		VKASSERT(vkWaitForFences(*device, count, handles.data(), VK_TRUE, timeout));
	}
}

void resetFence (ZFence fence)
{
	VkDevice device = *fence.getParam<ZDevice>();
	VKASSERT(vkResetFences(device, 1, fence.ptr()));
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
		VKASSERT(vkResetFences(*device, count, handles.data()));
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
		VKASSERT(vkResetFences(*device, count, handles.data()));
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

	VKASSERT(vkCreateSemaphore(*device, &semInfo, callbacks, &handle));

	return ZSemaphore::create(handle, device, callbacks);
}

ZQueryPool createQueryPool (ZDevice device, VkQueryType type, VkQueryPipelineStatisticFlags stats,
							uint32_t count, VkQueryPoolCreateFlags flags)
{
	auto		callbacks = device.getParam<VkAllocationCallbacksPtr>();
	VkQueryPool	queryPool = VK_NULL_HANDLE;
	VkQueryPoolCreateInfo	queryPoolInfo{};
	queryPoolInfo.sType					= VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.flags					= flags;
	queryPoolInfo.queryType				= type;
	queryPoolInfo.pipelineStatistics	= stats;
	queryPoolInfo.queryCount			= count;
	//add_cref<ZDeviceInterface> di = device.getInterface();
	VKASSERT(vkCreateQueryPool(*device, &queryPoolInfo, callbacks, &queryPool));
	// VUID - vkCmdCopyQueryPoolResults - None - 09402(ERROR / SPEC) : msgNum : -1161729443 - Validation Error :
	// [VUID - vkCmdCopyQueryPoolResults - None - 09402] Object 0 : type = VK_OBJECT_TYPE_QUERY_POOL;
	// | MessageID = 0xbac16a5d | vkCmdCopyQueryPoolResults() :
	// query not reset.After query pool creation, each query must be reset before it is used.
	// Queries must also be reset between uses. The Vulkan spec states:
	// All queries used by the command must not be uninitialized when the command is executed.
	//di.vkResetQueryPool(*device, queryPool, 0u, count);
	return ZQueryPool::create(queryPool, device, callbacks, count);
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
			default: ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
		}

		if ((features & featuresFlags) == featuresFlags)
			return *i;
	}
	ASSERTFALSE(""/*-Wgnu-zero-variadic-macro-arguments*/);
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

uint32_t enumeratePhysicalDevices (ZInstance instance, std::vector<VkPhysicalDevice>& devices)
{
	uint32_t deviceCount = 0;
	add_cref<ZInstanceInterface> ii = instance.getInterface();
	VKASSERT(ii.vkEnumeratePhysicalDevices(*instance, &deviceCount, nullptr));
	devices.resize(deviceCount, VkPhysicalDevice(0));
	VKASSERT(ii.vkEnumeratePhysicalDevices(*instance, &deviceCount, devices.data()));
	return deviceCount;
}

std::ostream& printPhysicalDevice (
	ZInstance instance,
	VkPhysicalDevice device,
	std::ostream& str,
	uint32_t deviceIndex)
{
	VkPhysicalDeviceProperties props{};
	instance.getInterface().vkGetPhysicalDeviceProperties(device, &props);
	return printPhysicalDevice(props, str, deviceIndex);
}

static strings enumerateDeviceExtensions(ZInstance instance, VkPhysicalDevice device, const char* layerName)
{
	strings		extensions;
	uint32_t	extensionCount = 0;

	VKASSERT(instance.getInterface().vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, nullptr));

	if (extensionCount)
	{
		extensions.resize(extensionCount);
		std::vector<VkExtensionProperties> props(extensionCount);
		VKASSERT(instance.getInterface().vkEnumerateDeviceExtensionProperties(device, layerName, &extensionCount, props.data()));
		std::transform(props.begin(), props.end(), extensions.begin(),
			[](VkExtensionProperties& p) {return std::string(p.extensionName); });
	}

	return extensions;
}

strings enumerateDeviceExtensions(ZInstance instance, VkPhysicalDevice device, const strings& layerNames)
{
	strings extensions = enumerateDeviceExtensions(instance, device, nullptr);
	for (const auto& layerName : layerNames)
	{
		mergeStringsDistinct(extensions, enumerateDeviceExtensions(instance, device, layerName.c_str()));
	}
	return extensions;
}

} // namespace vtf
