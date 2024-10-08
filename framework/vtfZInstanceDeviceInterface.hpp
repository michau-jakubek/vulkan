#ifndef __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__
#define __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__

#include "vtfVulkan.hpp"

namespace vtf
{
struct ZInstanceSingleton;
struct ZDeviceSingleton;

struct ZInstanceInterface
{
	friend	struct	ZInstanceSingleton;

protected:
			ZInstanceInterface	() : initialized(false) {}
	void	initialize			(VkInstance instance);
	bool	initialized;
};

struct ZDeviceInterface : public vk::DispatchLoaderDynamic
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

	bool shaderObject () const;

protected:
			ZDeviceInterface () : initialized(false) {}
	void	initialize		 (VkInstance instance, VkDevice device);
	bool	initialized;
};

struct ZInstanceSingleton
{
	virtual ~ZInstanceSingleton ();
	const ZInstanceInterface& getInterface (VkInstance instance);

protected:
	static ZInstanceInterface	m_interface;
};

struct ZDeviceSingleton
{
	const ZDeviceInterface& getInterface (VkInstance instance, VkDevice device);

protected:
	static ZDeviceInterface m_interface;
};


} // namespace vtf

#endif // __VTF_Z_INSTANCE_DEVICE_INTERFACE_HPP_INCLUDED__
