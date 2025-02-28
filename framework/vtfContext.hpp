#ifndef __VTF_CONTEXT_HPP_INCLUDED__
#define __VTF_CONTEXT_HPP_INCLUDED__

#include <initializer_list>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <vector>

#include "vulkan/vulkan.h"

#include "vtfZDeletable.hpp"
#include "vtfZUtils.hpp"
#include "vtfVertexInput.hpp"
#include "vtfThreadSafeLogger.hpp"

namespace vtf
{

class VulkanContext
{
public:
	virtual ~VulkanContext ();
	VulkanContext	(add_cptr<char>			appName,
					 add_cref<strings>		instanceLayers			= {},
					 add_cref<strings>		instanceExtensions		= {},
					 add_cref<strings>		deviceExtensions		= {},
					 OnEnablingFeatures		onEnablingFeatures		= {},
					 add_cref<Version>		apiVersion				= Version(1, 0),
					 bool					enableDebugPrintf		= false);

	VulkanContext	(ZInstance					anInstance,
					 ZPhysicalDevice			aPhysicalDevice,
					 ZDevice					aLogicalDevice);

	add_cref<VkAllocationCallbacksPtr>	callbacks;
	add_cref<ZInstance>					instance;
	add_cref<ZPhysicalDevice>			physicalDevice;
	add_cref<ZDevice>					device;
	add_cref<ZQueue>					graphicsQueue;
	add_cref<ZQueue>					computeQueue;
	uint32_t							getGraphicsQueueFamilyIndex () const;
	uint32_t							getComputeQueueFamilyIndex () const;
	add_cref<strings>					getAvailableInstanceExtensions () const;
	add_cref<strings>					getAvailablePhysicalDeviceExtensions () const;
	add_ref<Logger>						logger;

	ZCommandPool	createGraphicsCommandPool	();
	ZCommandPool	createComputeCommandPool	();
	ZImage			createColorImage2D			(VkFormat format, add_cref<VkExtent2D> extent, bool deviceAddress = false) const;
	ZImage			createColorImage2D			(VkFormat format, uint32_t width, uint32_t height, bool deviceAddress = false) const;

protected:
	VkAllocationCallbacksPtr			m_callbacks;
	ZInstance							m_instance;
	ZPhysicalDevice						m_physicalDevice;
	ZDevice								m_device;
	ZQueue								m_graphicsQueue;
	ZQueue								m_computeQueue;
	Logger								m_logger;
};

} // namespace vtf

#endif // __VTF_CONTEXT_HPP_INCLUDED__
