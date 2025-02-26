#ifndef __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__
#define __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__

#include "vtfVulkan.hpp"

#if VULKAN_HEADER_VERSION_MAJOR * 10 + VULKAN_HEADER_VERSION_MINOR < 14
typedef vk::DispatchLoaderDynamic DispatchLoaderDynamicClass;
#else
typedef vk::detail::DispatchLoaderDynamic DispatchLoaderDynamicClass;
#endif

namespace vtf
{
struct ZInstanceSingleton;
struct ZDeviceSingleton;

struct ZInstanceInterface : public DispatchLoaderDynamicClass
{
	friend	struct	ZInstanceSingleton;

protected:
			ZInstanceInterface	() : initialized(false) {}
	void	initialize			(VkInstance instance);
	bool	initialized;
};

struct ZDeviceInterface : public DispatchLoaderDynamicClass
{
	friend	struct	ZDeviceSingleton;

	static PFN_vkDestroyShaderEXT mDestroyShaderEXT;
	static inline void zDestroyShaderEXT(
		VkDevice                        device,
		VkShaderEXT                     shader,
		const VkAllocationCallbacks* pAllocator)
	{
		(*mDestroyShaderEXT)(device, shader, pAllocator);
	}

	bool isShaderObjectEnabled () const;

protected:
			ZDeviceInterface () : initialized(false) {}
	void	initialize		 (VkInstance instance, VkDevice device);
	bool	initialized;
};

struct ZInstanceSingleton
{
	virtual ~ZInstanceSingleton ();
	const ZInstanceInterface& getInterface () const;
protected:
	const ZInstanceInterface& initInterface (VkInstance instance);
	static ZInstanceInterface	m_interface;
};

struct ZDeviceSingleton
{
	const ZDeviceInterface& getInterface () const;

protected:
	const ZDeviceInterface& initInterface (VkInstance instance, VkDevice device);
	static ZDeviceInterface m_interface;
};


} // namespace vtf

#endif // __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__
