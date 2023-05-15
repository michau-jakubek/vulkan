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

namespace vtf
{

class VulkanContext
{
public:
	virtual ~VulkanContext ();
	VulkanContext	(const char*			appName,
					 const strings&			instanceLayers			= {},
					 const strings&			instanceExtensions		= {},
					 const strings&			deviceExtensions		= {},
					 GetEnabledFeaturesCB	onGetEnabledFeatures	= {},
					 uint32_t				apiVersion				= VK_API_VERSION_1_0,
					 bool					enableDebugPrintf		= false);

	VulkanContext	(VkAllocationCallbacksPtr	allocationCallbacks,
					 VkDebugUtilsMessengerEXT	debugMessengerHandle,
					 VkDebugReportCallbackEXT	debugReportHandle,
					 ZInstance					anInstance,
					 ZPhysicalDevice			aPhysicalDevice,
					 ZDevice					aLogicalDevice);

	VkAllocationCallbacksPtr&		callbacks;
	ZInstance&						instance;
	ZPhysicalDevice&				physicalDevice;
	ZDevice&						device;
	ZQueue							getGraphicsQueue () const;
	ZQueue							getComputeQueue () const;
	uint32_t						getGraphicsQueueFamilyIndex () const;
	uint32_t						getComputeQueueFamilyIndex () const;
	const strings&					getAvailableInstanceExtensions() const;
	const strings&					getAvailablePhysicalDeviceExtensions() const;
	VertexInput&					vertexInput;
	static uint32_t					deviceIndex;

	ZFence			createFence					(bool signaled = false);
	ZSemaphore		createSemaphore				();
	ZCommandPool	createGraphicsCommandPool	();
	ZCommandPool	createComputeCommandPool	();
	ZCommandBuffer	createCommandBuffer			(ZCommandPool commandPool);

	// Please keep in mind that if any of localSize[?] is valid value then it will correspond to
	// local size in shader layout respectively to layout(local_size_x_ID, local_size_y_ID, local_size_z_ID),
	// where local_size_*_ID must point to the values passed to an SpecID during creating compute pipeline.
	// Be carefull to set them properly according to their index in localSize vector.
	ZPipeline		createComputePipeline	(ZPipelineLayout layout, ZShaderModule computeShaderModule, const UVec3& localSize = UVec3(INVALID_UINT32), bool enableFullGroups = false);

	ZPipeline		createGraphicsPipeline	(ZPipelineLayout layout, ZRenderPass renderPass, std::optional<VkExtent2D> extent,
											 ZShaderModule vertShaderModule, ZShaderModule fragShaderModule,
											 std::optional<ZShaderModule> tessCtrlModule, std::optional<ZShaderModule> tessEvalModule, std::optional<ZShaderModule> geomModule,
											 VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, bool enableDepthTest = false, uint32_t patchControlPoints = 0,
											 VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL, std::initializer_list<VkDynamicState> dynamicStates = {});
	ZPipeline		createGraphicsPipeline	(ZPipelineLayout layout, ZRenderPass renderPass, std::optional<VkExtent2D> extent,
											 ZShaderModule vertShaderModule, ZShaderModule fragShaderModule, bool enableDepthTest = false);
	ZBuffer			createBuffer		(VkDeviceSize size, ZBufferUsageFlags usage = ZBufferUsageFlags::empty(),
										 ZMemoryPropertyFlags properties = { VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT }) const;
// TODO
//	ZBuffer			createBuffer		(ZImage image, ZBufferUsageFlags usage = ZBufferUsageFlags::empty(),
//										 uint32_t baseLevel = 0, uint32_t levels = INVALID_UINT32,
//										 ZMemoryPropertyFlags properties = { VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT }) const;
	ZImage			createImage2D		(VkFormat format, uint32_t width, uint32_t height, ZImageUsageFlags usage = { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
										 uint32_t mipLevels = 1, uint32_t layers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
										 VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) const;
	ZImageView		createImageView		(ZImage image,
										 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
										 VkFormat chgfmt = VK_FORMAT_UNDEFINED,
										 VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM,
										 uint32_t baseLevel = INVALID_UINT32, uint32_t levels = INVALID_UINT32,
										 uint32_t baseLayer = INVALID_UINT32, uint32_t layers = INVALID_UINT32) const;
	ZSampler		createSampler		(ZImageView view, bool filterLinearORnearest = true, bool normalized = true,
										 bool mipMapEnable = false, bool anisotropyEnable = false) const;
	ZFramebuffer	createFramebuffer	(ZRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<ZImageView>& attachments);
	ZRenderPass		createRenderPass	(std::vector<VkFormat> colorFormats,
										 std::optional<VkClearValue> clearColor = {},
										 VkImageLayout initialColorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
										 VkImageLayout finalColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										 bool enableDepthBuffer = false, float maxDepth = 1.0f);
	VkRenderPassBeginInfo	makeRenderPassBeginInfo(ZRenderPass rp, ZFramebuffer fb) const;

protected:
	VkAllocationCallbacksPtr	m_callbacks;
	VkDebugUtilsMessengerEXT	m_debugMessenger;
	VkDebugReportCallbackEXT	m_debugReport;
	ZInstance					m_instance;
	ZPhysicalDevice				m_physicalDevice;
	ZDevice						m_device;
	VertexInput					m_vertexInput;
};

//template<uint32_t AttachmentCount>
//ZFramebuffer VulkanContext::createFramebuffer (ZRenderPass renderPass, uint32_t width, uint32_t height, ZImageView const (&attachments)[AttachmentCount])
//{
//	ASSERTION(AttachmentCount > 0);
//	return createFramebufferImpl(renderPass, width, height, attachments, AttachmentCount);
//}

} // namespace vtf

#endif // __VTF_CONTEXT_HPP_INCLUDED__
