#ifndef __VTF_ZDELETABLE_HPP_INCLUDED__
#define __VTF_ZDELETABLE_HPP_INCLUDED__

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>
#include <ostream>

#include "vulkan/vulkan.h"
#include "vtfThreadSafeLogger.hpp"

#define NEXT_MAGIC() (__COUNTER__ + 1)

template<class C> using add_cst		= typename std::add_const<C>::type;
template<class P> using add_ptr		= typename std::add_pointer<P>::type;
template<class R> using add_ref		= typename std::add_lvalue_reference<R>::type;
template<class R> using add_rref	= typename std::add_rvalue_reference<R>::type;
template<class P> using add_cptr	= add_ptr<add_cst<P>>;
template<class R> using add_cref	= add_ref<add_cst<R>>;

template<class X> struct add_extent	{ typedef X type[]; };

VkAllocationCallbacks* getAllocationCallbacks();
void deletable_selfTest ();

void	assertion					(bool cond, const char* func, const char* file, int line, add_cref<std::string> msg = {});
bool	backtraceEnabled			();
void	backtraceEnabled			(bool enable);

void*	alloc_allocate				(size_t numBytes);
void	alloc_deallocate			(void* p, size_t numBytes);
size_t	alloc_get_allocation_count	();
size_t	alloc_get_allocation_size	();

#define ASSERTMSG(x,msg) assertion( !!(x), __func__, __FILE__, __LINE__, (msg))
#define VKASSERT(expr,msg) assertion( ((expr) == VK_SUCCESS), __func__, __FILE__, __LINE__, (msg))
#define ASSERTION(x) ASSERTMSG((x), std::string())
#define ASSERT_NOT_IMPLEMENTED() ASSERTMSG(false, "Not implemented yet")

template<class Z> struct ZAccess
{
	Z						object;
	bool					setOrGet;
	std::function<void(Z)>	callback;
	ZAccess(bool setOrGet, std::function<void(Z)> cb)
		: object	(VK_NULL_HANDLE)
		, setOrGet	(setOrGet)
		, callback	(cb)
	{
	}
	~ZAccess() { if (setOrGet) callback(object); }
	operator Z*() { return &object; }
};

struct ZDeletableMark { };

template<int I> struct ParamTraits
{
	// needs a dereferencing
	template<size_t J, class Params> using is_zdeletable =
		std::is_convertible<add_ptr<std::tuple_element_t<J, Params>>, add_ptr<ZDeletableMark>>;

	template<class Z, class Params, typename std::enable_if<is_zdeletable<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params) {
		return *std::get<I>(params);
	}

	template<class Z, class Params, typename std::enable_if<!is_zdeletable<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params) {
		return std::get<I>(params);
	}
};

template<> struct ParamTraits<-1>
{
	template<class Z, class Params>
	auto operator()(Z z, const Params&)	{
		return z;
	}
};

template<class Object, int... I>
struct ZObjectDeleter
{
	void operator()(Object* obj) const
	{
		/* if (obj->handle != VK_NULL_HANDLE && obj->routine != nullptr) {
			std::invoke(obj->routine, ParamTraits<I>()(obj->handle, obj->params)...);
		} */
		obj->template destroy<I...>();
	}
};

template<class Object, class Indexes> struct ZDeleterTraits;
template<class Object, int... I>
	struct ZDeleterTraits<Object, std::integer_sequence<int, I...>>
	{
		typedef ZObjectDeleter<Object, I...> type;
	};

template<class Z, class F, class P>
struct ZObject
{
	Z	handle;
	F	routine;
	P	params;
	ZObject(F ptr)
		: handle	(VK_NULL_HANDLE)
		, routine	(ptr)
		, params	() { }
	ZObject(Z z, F ptr, add_cref<P> params)
		: handle	(z)
		, routine	(ptr)
		, params	(params) { }
	template<int... I> void destroy() {
		if (handle != VK_NULL_HANDLE && routine != nullptr) {
			std::invoke(routine, ParamTraits<I>()(handle, params)...);
		}
	}
};

template<class Z, class F, F Ptr, class Tr, class... X> struct ZDeletable
	: protected std::shared_ptr<ZObject<Z, F, std::tuple<X...>>>, public ZDeletableMark
{
	static_assert(std::is_pointer<Z>::value, "Z must be of pointer type");

	typedef Z											handle_type;
	typedef ZObject<Z, F, std::tuple<X...>>				AnObject;
	typedef std::shared_ptr<AnObject>					super;
	typedef typename ZDeleterTraits<AnObject, Tr>::type	ADeleter;
	ZDeletable()	: std::shared_ptr<AnObject>(
						  new AnObject(Ptr)
						  , ADeleter())
					, ZDeletableMark() { /* Intentionally empty */ }
	ZDeletable(Z z, add_cref<X>... x)	: std::shared_ptr<AnObject>(
							  new AnObject(z, Ptr, std::make_tuple(x...))
							  , ADeleter())
						, ZDeletableMark() { /* Intentionally empty */ }
	virtual ~ZDeletable() = default;
	static ZDeletable create(Z z, add_cref<X>... x)
	{
		return ZDeletable(z, x...);
	}
	ZAccess<Z> setter()
	{
		return ZAccess<Z>(true, std::bind(&ZDeletable::replace, this, std::placeholders::_1));
	}	
	bool has_handle () const
	{
		add_cptr<AnObject> obj = super::get();
		ASSERTMSG(nullptr != obj, "Object must not be null");
		return (VK_NULL_HANDLE != obj->handle);
	}
	add_cptr<Z> ptr() const
	{
		add_cptr<AnObject> obj = super::get();
		ASSERTMSG(obj != nullptr, "Object must not be null");
		add_cptr<Z> p = &obj->handle;
		ASSERTMSG(VK_NULL_HANDLE != *p, "Vulkan handle must not be VK_NULL_HANDLE");
		return p;
	}
	Z operator*() const
	{
		return *ptr();
	}
	bool operator==(add_cref<ZDeletable> other) const
	{
		return (super::get()->handle == static_cast<add_cref<super>>(other).get()->handle);
	}
	template<class T> T getParam() const
	{
		return std::get<T>(super::get()->params);
	}
	template<class T> add_ref<T> getParamRef()
	{
		return std::get<T>(super::get()->params);
	}
	template<class T> add_cref<T> getParamRef() const
	{
		return std::get<T>(super::get()->params);
	}
	template<class T> void setParam(add_cref<T> param)
	{
		std::get<T>(super::get()->params) = param;
	}
	long use_count() const noexcept
	{
		return super::use_count();
	}
	std::ostream& operator<<(std::ostream& str) const
	{
		return (str << static_cast<void*>(super::get()->handle));
	}
	friend std::ostream& operator<<(std::ostream& str, add_cref<ZDeletable> z)
	{
		return (str << static_cast<void*>(static_cast<add_cref<super>>(z).get()->handle));
	}
private:
	void replace(Z z)
	{
		add_ptr<AnObject> obj = super::get();
		ASSERTION(VK_NULL_HANDLE == obj->handle);
		obj->handle = z;
	}
};

/*
 * Controls the params passed to a deleter routine from a tuple.
 * (-1) means that Vulkan handle from ZDeletable
 * otherwise parameter at desired index will be passed.
 */
typedef std::integer_sequence<int>				swizzle_no_param;
typedef std::integer_sequence<int, -1>			swizzle_one_param;
typedef std::integer_sequence<int, -1, 0>		swizzle_two_param;
typedef std::integer_sequence<int, 0, -1, 1>	swizzle_three_params;

enum ZDistName
{
	None,
	RequiredLayers,				AvailableLayers,
	RequiredLayerExtensions,	AvailableLayerExtensions,
	RequiredDeviceExtensions,	AvailableDeviceExtensions,
	Width, Height, Depth,		PatchControlPoints, SubpassIndex,
	QueueFamilyIndex, QueueIndex, QueueFlags, QueueCreateInfoList, QueueList,
	CullModeFlags, DepthTestEnable, DepthWriteEnable, StencilTestEnable,
	LineWidth, AttachmentCount, SubpassCount, ViewportCount, ScissorCount,
};
template<ZDistName, class CType_>
struct ZDistType
{
	CType_ data;
	ZDistType () : data() {}
	ZDistType (const CType_& v) : data(v) {}
	operator add_ref<CType_> () { return data; }
	operator add_cref<CType_> () const { return data; }
};

typedef add_ptr<VkAllocationCallbacks> VkAllocationCallbacksPtr;

typedef ZDeletable<VkInstance,
	decltype(&vkDestroyInstance), &vkDestroyInstance,
	swizzle_two_param, VkAllocationCallbacksPtr, /*apiVersion*/ uint32_t,
	ZDistType<RequiredLayers, std::vector<std::string>>,
	ZDistType<AvailableLayerExtensions, std::vector<std::string>>,
	vtf::Logger>
ZInstance;

typedef ZDeletable<VkPhysicalDevice,
	void(*)(void), nullptr
	, swizzle_no_param
	, VkAllocationCallbacksPtr
	, ZInstance
	, uint32_t /*Physical device index across the system*/
	, std::vector<std::string> /*Device extensions*/
	, VkPhysicalDeviceProperties>
ZPhysicalDevice;

typedef ZDeletable<VkQueue,
	void(*)(void), nullptr
	, swizzle_no_param
	, ZDistType<QueueFamilyIndex, uint32_t>
	, ZDistType<QueueIndex, uint32_t>
	, ZDistType<QueueFlags, VkQueueFlags>
	, bool /*surfaceSupported*/>
ZQueue;

struct VkDeviceQueueCreateInfoEx : VkDeviceQueueCreateInfo
{
	VkQueueFlags	queueFlags;
	bool			surfaceSupport;
};
typedef ZDeletable<VkDevice,
	decltype(&vkDestroyDevice), &vkDestroyDevice,
	swizzle_two_param, VkAllocationCallbacksPtr,
	ZPhysicalDevice,
	ZDistType<QueueCreateInfoList, std::vector<VkDeviceQueueCreateInfoEx>>,
	ZDistType<QueueList, std::vector<ZQueue>>
>
ZDevice;

struct GLFWwindow;
void freeWindow(add_ptr<GLFWwindow>);
typedef typename std::add_pointer<GLFWwindow>::type GLFWwindowPtr;
typedef ZDeletable<GLFWwindowPtr,
	decltype(&freeWindow), &freeWindow,
	swizzle_one_param>
ZGLFWwindowPtr;

typedef ZDeletable<VkSurfaceKHR,
	decltype(&vkDestroySurfaceKHR), &vkDestroySurfaceKHR,
	swizzle_three_params, ZInstance, VkAllocationCallbacksPtr,
	ZGLFWwindowPtr>
ZSurfaceKHR;

typedef ZDeletable<VkSwapchainKHR,
	decltype(&vkDestroySwapchainKHR),	&vkDestroySwapchainKHR,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr>
ZSwapchainKHR;

typedef ZDeletable<VkShaderModule,
	decltype(&vkDestroyShaderModule), &vkDestroyShaderModule,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkShaderStageFlagBits>
ZShaderModule;

typedef ZDeletable<VkCommandPool,
	decltype(&vkDestroyCommandPool), &vkDestroyCommandPool,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	ZQueue>
ZCommandPool;

inline void freeCommandBuffer (VkDevice device, VkCommandBuffer buff, VkCommandPool pool) {
	vkFreeCommandBuffers(device, pool, 1, &buff); }
typedef ZDeletable<VkCommandBuffer,
	decltype(&freeCommandBuffer), &freeCommandBuffer,
	swizzle_three_params, ZDevice, ZCommandPool, bool>
ZCommandBuffer;

typedef ZDeletable<VkFence,
	decltype(&vkDestroyFence), &vkDestroyFence,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr>
ZFence;

typedef ZDeletable<VkSemaphore,
	decltype(&vkDestroySemaphore), &vkDestroySemaphore,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr>
ZSemaphore;

typedef ZDeletable<VkDeviceMemory,
	decltype(&vkFreeMemory), &vkFreeMemory,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkMemoryPropertyFlags, VkDeviceSize, uint8_t*>
ZDeviceMemory;

typedef ZDeletable<VkImage,
	decltype(&vkDestroyImage), &vkDestroyImage,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkImageCreateInfo, ZDeviceMemory, VkDeviceSize>
ZImage;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkImageViewCreateInfo, ZImage>
ZImageView;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr>
ZUnboundImageView;

typedef ZDeletable<VkRenderPass,
	decltype(&vkDestroyRenderPass), &vkDestroyRenderPass,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<AttachmentCount, uint32_t>,
	ZDistType<SubpassCount, uint32_t>,
	std::vector<VkClearValue>,
	bool, /*enable depth attachment*/
	VkImageLayout /*finalLayout*/>
ZRenderPass;

typedef ZDeletable<VkFramebuffer,
	decltype(&vkDestroyFramebuffer), &vkDestroyFramebuffer,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<Width, uint32_t>, ZDistType<Height, uint32_t>,
	ZRenderPass, std::vector<ZImageView>>
ZFramebuffer;

typedef ZDeletable<VkSampler,
	decltype(&vkDestroySampler), &vkDestroySampler,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkSamplerCreateInfo>
ZSampler;

struct type_index_with_default : public std::type_index
{
	type_index_with_default () : std::type_index(typeid(void)) {}
	type_index_with_default (add_cref<std::type_info> info) : std::type_index(info) {}
	template<class X> static type_index_with_default make () {
		return type_index_with_default(typeid(X));
	}
};

typedef ZDeletable<VkBuffer,
	decltype(&vkDestroyBuffer), &vkDestroyBuffer,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	VkBufferCreateInfo, ZDeviceMemory, VkDeviceSize,
	type_index_with_default, VkExtent3D, VkFormat>
ZBuffer;

typedef ZDeletable<VkDescriptorPool,
	decltype(&vkDestroyDescriptorPool), &vkDestroyDescriptorPool,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr>
ZDescriptorPool;

inline void freeDescriptorSet (VkDevice device, VkDescriptorSet set, VkDescriptorPool pool) {
	vkFreeDescriptorSets(device, pool, 1, &set); }
typedef ZDeletable<VkDescriptorSet,
	decltype(&freeDescriptorSet), &freeDescriptorSet,
	swizzle_three_params, ZDevice, ZDescriptorPool>
ZDescriptorSet;

typedef ZDeletable<VkDescriptorSetLayout,
	decltype(&vkDestroyDescriptorSetLayout), &vkDestroyDescriptorSetLayout,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	ZDescriptorSet>
ZDescriptorSetLayout;

typedef ZDeletable<VkPipelineLayout,
	decltype(&vkDestroyPipelineLayout), &vkDestroyPipelineLayout,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	std::vector<ZDescriptorSetLayout>,
	VkPushConstantRange, type_index_with_default>
ZPipelineLayout;

typedef ZDeletable<VkPipeline,
	decltype(&vkDestroyPipeline), &vkDestroyPipeline,
	swizzle_three_params, ZDevice, VkAllocationCallbacksPtr,
	ZPipelineLayout, ZRenderPass, VkPipelineBindPoint>
ZPipeline;

#endif // __VTF_ZDELETABLE_HPP_INCLUDED__
 
