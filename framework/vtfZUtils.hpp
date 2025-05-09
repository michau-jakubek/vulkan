#ifndef __VTF_Z_UTILS_HPP_INCLUDED__
#define __VTF_Z_UTILS_HPP_INCLUDED__

#include <cstddef>
#include <optional>
#include <ostream>
#include <type_traits>
#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZBarriers.hpp"
#include "vtfExtensions.hpp"
#if SYSTEM_OS_LINUX == 1
  #include "demangle.hpp"
#endif

#if SYSTEM_OS_WINDOWS == 1
 #if SHARED_LIBRARY == 1
  #define SHARED_RESOURCE __declspec(dllexport)
 #else
  #define SHARED_RESOURCE __declspec(dllimport)
 #endif
#else
 #define SHARED_RESOURCE extern
#endif

namespace vtf
{

class VertexInput;

Version			getVulkanImplVersion (std::optional<ZInstance> instance = {});

ZDevice			getSharedDevice ();
ZInstance		getSharedInstance ();
ZPhysicalDevice	getSharedPhysicalDevice ();

strings			upgradeInstanceExtensions (add_cref<strings> desiredExtensions);
ZInstance		createInstance (const char*							appName,
								VkAllocationCallbacksPtr			callbacks,
								const strings&						desiredLayers = {},
								const strings&						desiredExtensions = {},
								uint32_t							apiVersion = VK_API_VERSION_1_1,
								bool								enableDebugPrintf = false);
ZPhysicalDevice	getPhysicalDeviceByIndex	(ZInstance									instance,
											 uint32_t									physicalDeviceIndex);
ZPhysicalDevice selectPhysicalDevice		(const int									proposedDeviceIndex,
											 ZInstance									instance,
											 add_cref<strings>							requiredExtensions,
											 ZSurfaceKHR								surface = ZSurfaceKHR());

strings			upgradeDeviceExtensions (add_cref<strings> desiredExtensions);
typedef std::function<void(add_ref<DeviceCaps>)> OnEnablingFeatures;
ZDevice			createLogicalDevice	(ZPhysicalDevice		physDevice,
									 OnEnablingFeatures		onEnablingFeatures,
									 ZSurfaceKHR			surface = ZSurfaceKHR(),
									 bool					enableDebugPrintf = false);
add_cref<VkPhysicalDeviceProperties> deviceGetPhysicalProperties (add_cref<ZDevice> device);
add_cref<VkPhysicalDeviceProperties> deviceGetPhysicalProperties (add_cref<ZPhysicalDevice> device);
VkPhysicalDeviceProperties2			 deviceGetPhysicalProperties2 (add_cref<ZDevice> device, add_ptr<void> pNext = nullptr);
template<typename  PropertiesType, typename FieldType, typename MaskType>
FieldType deviceCheckProperties (
	ZPhysicalDevice dev,
	FieldType PropertiesType::* pField,
	bool anyBit = true,
	MaskType mask = {},
	bool exceptionOnFail = true,
	add_cptr<char> fieldNameAndDescription = {})
{
	PropertiesType pp = makeVkStruct();
	VkPhysicalDeviceProperties2 p2 = makeVkStruct(&pp);
	vkGetPhysicalDeviceProperties2(*dev, &p2);
	const FieldType fieldValue = pp.*pField;
	if (exceptionOnFail) {
		ASSERTMSG(anyBit
			? std::bitset<sizeof(FieldType) * 8>(fieldValue).any()
			: ((fieldValue & FieldType(mask)) == FieldType(mask)),
				demangledName<PropertiesType>(), "::",
					(fieldNameAndDescription ? fieldNameAndDescription : ""));
	}
	return fieldValue;
}
add_cref<VkPhysicalDeviceLimits>
				deviceGetPhysicalLimits (add_cref<ZDevice> device);
add_cref<VkPhysicalDeviceLimits>
				deviceGetPhysicalLimits (add_cref<ZPhysicalDevice> device);
VkPhysicalDeviceFeatures2
				deviceGetPhysicalFeatures2 (ZPhysicalDevice device, void* pNext = nullptr);
ZPhysicalDevice	deviceGetPhysicalDevice (ZDevice device);
ZInstance		deviceGetInstance (ZPhysicalDevice device);
ZInstance		deviceGetInstance (ZDevice device);
ZQueue			deviceGetNextQueue			(ZDevice device, VkQueueFlags queueFlags, bool mustSupportSurface);
uint32_t		queueGetFamilyIndex			(ZQueue queue);
uint32_t		queueGetIndex				(ZQueue queue);
VkQueueFlags	queueGetFlags				(ZQueue queue);
bool			queueSupportSwapchain		(ZQueue queue);

ZFence			createFence		(ZDevice device, bool signaled = false);
VkResult		waitForFence	(ZFence fence, uint64_t timeout = UINT64_MAX, bool assertOnFail = true);
void			resetFence		(ZFence fence);
bool			fenceStatus		(ZFence fence);
void			waitForFences	(std::initializer_list<ZFence> fences, uint64_t timeout = UINT64_MAX);
void			resetFences		(std::initializer_list<ZFence> fences);
void			waitForFences	(std::vector<ZFence> fences, uint64_t timeout = UINT64_MAX);
void			resetFences		(std::vector<ZFence> fences);
bool			fenceStatus		(ZFence fence);
ZSemaphore		createSemaphore	(ZDevice device);

ZQueryPool		createQueryPool	(ZDevice device, VkQueryType type, VkQueryPipelineStatisticFlags stats,
								 uint32_t count = 1u, VkQueryPoolCreateFlags flags = 0);

ZShaderModule	createShaderModule (ZDevice device, VkShaderStageFlagBits stage,
                                    add_cref<std::vector<char>> code, add_cref<std::string> entryName);

ZFramebuffer	createFramebuffer (ZRenderPass renderPass, add_cref<VkExtent2D> size, const std::vector<ZImageView>& attachments);
ZFramebuffer	createFramebuffer (ZRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<ZImageView>& attachments);

struct ZRenderPassBeginInfo : protected VkRenderPassBeginInfo
{
	ZRenderPassBeginInfo (ZCommandBuffer, ZFramebuffer, uint32_t subpass, VkSubpassContents);
	ZRenderPassBeginInfo (ZCommandBuffer, ZRenderPass, ZFramebuffer, uint32_t subpass, VkSubpassContents);
	VkRenderPassBeginInfo operator ()() const;
	ZCommandBuffer		getCommandBuffer	() const { return m_cmdBuffer; }
	ZFramebuffer		getFramebuffer		() const { return m_framebuffer; }
	ZRenderPass			getRenderPass		() const { return m_renderPass; }
	uint32_t			consumeSubpass		() { return m_subpass++; }
	uint32_t			getSubpass			() const { return m_subpass; }
	VkSubpassContents	getContents			() const { return m_contents; }
protected:
	ZCommandBuffer		m_cmdBuffer;
	ZFramebuffer		m_framebuffer;
	ZRenderPass			m_renderPass;
	uint32_t			m_subpass;
	VkSubpassContents	m_contents;
};
/**
* @device			A device which render pass is created
* @colorFormats		Defines a subsequent attachments.
*                   If colorFormats[attachment].alpha is <> 0 then attachment is OP_CLEAR
*                   otherwise attachment is considered as OP_LOAD.
*/
ZRenderPass		createColorRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
									   std::vector<VkClearValue> clearColors = {},
									   VkImageLayout initialColorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   VkImageLayout finalColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									   std::initializer_list<ZSubpassDependency> deps = {});
/*
* If depthStencilFormat is not VK_FORMAT_UNDEFINED then depth-stencil attachment
* will be automatically added to the end of the list of all color attachments
*/
ZRenderPass		createColorRenderPass (ZDevice device, VkFormat depthStencilFormat,
									   add_cref<std::vector<VkFormat>> colorFormats,
									   std::vector<VkClearValue> clearColors = {},
									   VkImageLayout initialColorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   VkImageLayout finalColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									   std::initializer_list<ZSubpassDependency> deps = {});
ZRenderPass		createMultiViewRenderPass (ZDevice device, add_cref<std::vector<VkFormat>> colorFormats,
										   std::vector<VkClearValue> clearColors = {},
										   std::initializer_list<ZSubpassDependency> dependencies = {},
										   VkImageLayout initialColorLayout = VK_IMAGE_LAYOUT_UNDEFINED,
										   VkImageLayout finalColorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

ZRenderPass		framebufferGetRenderPass (ZFramebuffer framebuffer);
ZImageView		framebufferGetView (ZFramebuffer framebuffer, uint32_t index = 0);
ZImage			framebufferGetImage	(ZFramebuffer framebuffer, uint32_t index = 0);

VkFormat		selectBestDepthStencilFormat (ZPhysicalDevice					device,
											  std::initializer_list<VkFormat>	formats,
											  VkImageTiling						imageTiling,
											  VkFormatFeatureFlags				featuresFlags);

bool			pointInTriangle2D (const Vec2& p, const Vec2& p0, const Vec2& p1, const Vec2& p2, bool bar = false);
bool			pointInTriangle2D (const Vec3& p, const Vec3& p0, const Vec3& p1, const Vec3& p2, bool bar = false);
bool			pointInTriangle2D (const Vec4& p, const Vec4& p0, const Vec4& p1, const Vec4& p2, bool bar = false);

uint32_t enumeratePhysicalDevices (ZInstance instance, std::vector<VkPhysicalDevice>& devices);
std::ostream& printPhysicalDevice (ZInstance instance, VkPhysicalDevice device, std::ostream& str, uint32_t deviceIndex);
strings enumerateDeviceExtensions(ZInstance instance, VkPhysicalDevice device, const strings& layerNames);


/*
template<class PixelType, class MapFun>
std::ostream& printImageLeveled (ZCommandPool commandPool, ZImage image, MapFun&& mapFun, std::ostream& str = std::cout)
{
	ZBuffer		buffer			= createBuffer(image);
	copyImageToBuffer(commandPool, image, buffer);
	auto		pixels			= readBuffer<PixelType>(buffer);
	const auto	ci				= viewportImage.getParam<VkImageCreateInfo>();

	int			levelOffset		= 0;
	int			width			= ci.extent.width;
	int			height			= ci.extent.height;
	const int	availableLevels = ci.mipLevels;

	str << "printImageLeveled::pixelCount: " << pixels.size() << std::endl;

	for (int level = 0; level < availableLevels; ++level)
	{
		str << "Level: " << level << " " << width << "x" << height << ' ' << " pixels: " << pixels.size() << std::endl;

		for (int y = height-1; y >= 0; --y)
		{
			for (int x = 0; x < width; ++x)
			{
				auto px = pixels[((y * width) + x) + levelOffset];
				str << mapFun(px) << ' ';
				//std::cout << px << ' ';
			}
			str << std::endl;
		}
		levelOffset += (width * height);
		width /= 2;
		height /= 2;
	}

	str << "Offset: " << levelOffset << std::endl;
	return str;
}
*/

} // namespace vtf

#endif // __VTF_Z_UTILS_HPP_INCLUDED__
