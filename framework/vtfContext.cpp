#include <array>
#include <algorithm>
#include <iostream>
#include <sstream>
#include "vtfZUtils.hpp"
#include "vtfContext.hpp"
#include "vtfZImage.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfDebugMessenger.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{

VulkanContext::VulkanContext (ZInstance					anInstance,
							  ZPhysicalDevice			aPhysicalDevice,
							  ZDevice					aLogicalDevice)
	// begining references initialization
	: callbacks						(m_callbacks)
	, instance						(m_instance)
	, physicalDevice				(m_physicalDevice)
	, device						(m_device)
	, graphicsQueue					(m_graphicsQueue)
	, computeQueue					(m_computeQueue)
	, logger						(m_logger)
	// end of references initialization
	, m_callbacks					(anInstance.getParam<VkAllocationCallbacksPtr>())
	, m_instance					(anInstance)
	, m_physicalDevice				(aPhysicalDevice)
	, m_device						(aLogicalDevice)
	, m_graphicsQueue				(deviceGetNextQueue(m_device, VK_QUEUE_GRAPHICS_BIT, false))
	, m_computeQueue				((queueGetFlags(m_graphicsQueue) & VK_QUEUE_COMPUTE_BIT)
										? m_graphicsQueue
										: deviceGetNextQueue(m_device, VK_QUEUE_COMPUTE_BIT, false))
	, m_logger						(m_instance.getParamRef<Logger>())
{
}

VulkanContext::VulkanContext (add_cptr<char>		appName,
							  add_cref<strings>		instanceLayers,
							  add_cref<strings>		instanceExtensions,
							  add_cref<strings>		deviceExtensions,
							  OnEnablingFeatures	onEnablingFeatures,
							  add_cref<Version>		apiVersion,
							  bool					enableDebugPrintf)
	// begining references initialization
	: callbacks						(m_callbacks)
	, instance						(m_instance)
	, physicalDevice				(m_physicalDevice)
	, device						(m_device)
	, graphicsQueue					(m_graphicsQueue)
	, computeQueue					(m_computeQueue)
	, logger						(m_logger)
	// end of initialization of references
	, m_callbacks					(getAllocationCallbacks())
	, m_instance					(getSharedInstance()
									 | ([&,this]() -> ZInstance
										{
									 return createInstance(appName, m_callbacks, instanceLayers, upgradeInstanceExtensions(instanceExtensions),
															apiVersion, enableDebugPrintf);}))
	, m_physicalDevice				(getSharedPhysicalDevice()
									 | selectPhysicalDevice(make_signed(getGlobalAppFlags().physicalDeviceIndex), m_instance, deviceExtensions))
	, m_device						(getSharedDevice() | createLogicalDevice(
										 m_physicalDevice, onEnablingFeatures, ZSurfaceKHR(), enableDebugPrintf))
	, m_graphicsQueue				(deviceGetNextQueue(m_device, VK_QUEUE_GRAPHICS_BIT, false))
	, m_computeQueue				((queueGetFlags(m_graphicsQueue) & VK_QUEUE_COMPUTE_BIT)
										? m_graphicsQueue
										: deviceGetNextQueue(m_device, VK_QUEUE_COMPUTE_BIT, false))
	, m_logger						(m_instance.getParamRef<Logger>())
{
}

VulkanContext::~VulkanContext ()
{
	if (getGlobalAppFlags().verbose)
	{
		std::cout << "[INFO] Destructor " << __func__ << ' '
				  << device << '(' << device.use_count() << ") "
				  << physicalDevice << '(' << physicalDevice.use_count() << ") "
				  << instance << '(' << instance.use_count() << ") "
				  << std::endl;
	}
}

uint32_t VulkanContext::getGraphicsQueueFamilyIndex () const
{
	return queueGetFamilyIndex(m_graphicsQueue);
}

uint32_t VulkanContext::getComputeQueueFamilyIndex () const
{
	return queueGetFamilyIndex(m_computeQueue);
}

const strings& VulkanContext::getAvailableInstanceExtensions() const
{
	return  instance.getParamRef<ZDistType<AvailableLayerExtensions, strings>>();
}

const strings& VulkanContext::getAvailablePhysicalDeviceExtensions() const
{
	return physicalDevice.getParamRef<ZDistType<AvailableDeviceExtensions, strings>>();
}

ZCommandPool VulkanContext::createGraphicsCommandPool ()
{
	return createCommandPool(device, graphicsQueue);
}

ZCommandPool VulkanContext::createComputeCommandPool ()
{
	return createCommandPool(device, computeQueue);
}

ZImage	VulkanContext::createColorImage2D (VkFormat format, add_cref<VkExtent2D> extent, bool deviceAddress) const
{
	return createColorImage2D(format, extent.width, extent.height, deviceAddress);
}

ZImage	VulkanContext::createColorImage2D (VkFormat format, uint32_t width, uint32_t height, bool deviceAddress) const
{
	const ZImageUsageFlags usage = { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
									 VK_IMAGE_USAGE_STORAGE_BIT,
									 VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
									 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
									 VK_IMAGE_USAGE_TRANSFER_DST_BIT };
	const uint32_t mipLevels = 1;
	const uint32_t layers = 1;
	const uint32_t depth = 1;
	const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	const ZMemoryPropertyFlags properties = ZMemoryPropertyDeviceFlags;

	return createImage(device, format, VK_IMAGE_TYPE_2D, width, height, usage, samples, mipLevels, layers, depth,
						deviceAddress, properties);
}

} // namespace vtf
