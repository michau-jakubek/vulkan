#include "vtfZUtils.hpp"
#include "vtfBacktrace.hpp"

namespace vtf
{
int k;

#if VTF_AS_DLL
static std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<VtfAppSharedObject>> m_vtfAppsharedObjects;
add_ref<std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<VtfAppSharedObject>>>
VtfAppSharedObject::get() { return m_vtfAppsharedObjects; }
VtfAppSharedObject::Key getKeyFromGlobalAppFlags()
{
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	const VtfAppSharedObject::Key key({ gf.vtfAsDllInstance, gf.physicalDeviceIndex });
	ASSERTMSG(mapHasKey(m_vtfAppsharedObjects, key),
		"Unable to find (instance = ", key.first, ", device = ", key.second, ") pair");
	return key;
}
#endif // VTF_AS_DLL

ZDevice getSharedDevice()
{
#if VTF_AS_DLL
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	if (std::string_view(gf.cmdSignature) != "VTF_AS_DLL") return {};
	const auto key = getKeyFromGlobalAppFlags();
	auto p = m_vtfAppsharedObjects[key];
	if (false == bool(p->zDevice))
	{
		ZPhysicalDevice phys = getSharedPhysicalDevice();
		ZInstance instance = phys.getParam<ZInstance>();

		std::vector<ZDeviceQueueCreateInfo> qs(2, {});

		qs[0].queueFlags = VK_QUEUE_GRAPHICS_BIT;
		qs[0].queueFamilyIndex = p->graphicsQueueFamilyIndex;
		qs[0].queueCount = 1;
		qs[0].surfaceSupport = true;
		qs[0].queues.set(0);

		qs[1].queueFlags = VK_QUEUE_COMPUTE_BIT;
		qs[1].queueFamilyIndex = p->computeQueueFamilyIndex;
		qs[1].queueCount = 1;
		qs[1].queues.set(0);

		ZDevice device(p->device, getAllocationCallbacks(), phys, 1, qs);

		struct AInstance : public ZInstance {
			struct ADevice : public ZDevice {
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
		AInstance(instance).initInterface(device);

		p->zDevice = device;
	}
	return p->zDevice;
#endif // VTF_AS_DLL
	return {};
}

ZPhysicalDevice getSharedPhysicalDevice()
{
#if VTF_AS_DLL
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	if (std::string_view(gf.cmdSignature) != "VTF_AS_DLL") return {};
	const auto key = getKeyFromGlobalAppFlags();
	auto p = m_vtfAppsharedObjects[key];
	if (false == bool(p->zPhys))
	{
		ZInstance instance = getSharedInstance();
		add_cref<ZInstanceInterface> ii = instance.getInterface();
		VkPhysicalDeviceProperties props{};
		ii.vkGetPhysicalDeviceProperties(p->phys, &props);
		add_cref<strings> layers = instance.getParamRef<ZDistType<RequiredLayers, strings>>();
		strings availableDeviceExtensions = enumerateDeviceExtensions(instance, p->phys, layers);
		ZPhysicalDevice phys(
			p->phys,
			instance.getParam<VkAllocationCallbacksPtr>(),
			instance, p->deviceID,
			std::move(availableDeviceExtensions),
			gf.deviceExtensions, // RequiredDeviceExtensions
			props);
		p->zPhys = phys;
	}
	return p->zPhys;
#else
	return {};
#endif // VTF_AS_DLL
}

ZInstance getSharedInstance()
{
#if VTF_AS_DLL
	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	if (std::string_view(gf.cmdSignature) != "VTF_AS_DLL") return {};
	const auto key = getKeyFromGlobalAppFlags();
	auto p = m_vtfAppsharedObjects[key];
	if (false == bool(p->zInstance))
	{
		ZInstance instance(p->instance
			, getAllocationCallbacks()
			, 1 // Undeletable
			, "VTF"
			, gf.apiVer
			, enumerateInstanceLayers() // AvailableLayers
			, gf.layers // RequiredLayers
			, enumerateInstanceExtensions(gf.layers) // AvailableLayerExtensions
			, std::move(p->requiredInstanceExtensions) // RequiredLayerExtensions
			, Logger()
			, ProgressRecorder()
			, VkDebugUtilsMessengerEXT(VK_NULL_HANDLE)
			, VkDebugReportCallbackEXT(VK_NULL_HANDLE)
		);

		struct AInstance : public ZInstance {
			AInstance(ZInstance i) : ZInstance(i) {}
			void initInterface() {
				ZInstanceSingleton::initInterface(this->operator*());
			}
		};
		AInstance(instance).initInterface();

		p->zInstance = instance;
	}
	return p->zInstance;
#else
	return {};
#endif // VTF_AS_DLL
}

} // namespace vtf
