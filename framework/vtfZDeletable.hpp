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

#include "vulkan/vulkan.h"

#define NEXT_MAGIC() (__COUNTER__ + 1)

VkAllocationCallbacks* getAllocationCallbacks();
void deletable_selfTest ();

void	assertion					(bool cond, const char* func, const char* file, int line, const std::string msg = {});
bool	backtraceEnabled			();
void	backtraceEnabled			(bool enable);

void*	alloc_allocate				(size_t numBytes);
void	alloc_deallocate			(void* p, size_t numBytes);
size_t	alloc_get_allocation_count	();
size_t	alloc_get_allocation_size	();

#define ASSERTMSG(x,msg) assertion( !!(x), __func__, __FILE__, __LINE__, (msg))
#define VKASSERT(expr,msg) assertion( ((expr) == VK_SUCCESS), __func__, __FILE__, __LINE__, (msg))
#define ASSERTION(x) ASSERTMSG((x), std::string())

template<class C> using add_cst		= typename std::add_const<C>::type;
template<class P> using add_ptr		= typename std::add_pointer<P>::type;
template<class R> using add_ref		= typename std::add_lvalue_reference<R>::type;
template<class P> using add_cptr	= add_ptr<add_cst<P>>;
template<class R> using add_cref	= add_ref<add_cst<R>>;

template<class X> struct add_extent
{
	typedef X type[];
};

template<class X, uint32_t I>
struct TypeWrapper
{
	X field;
	TypeWrapper() : field() {}
	TypeWrapper(const X& x) : field(x) {}
	TypeWrapper(const TypeWrapper& other) : field(other.field) {}
	operator X& () { return field; }
	X& operator()() { return field; }
};

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
	template<size_t J, class Params> using needs_dereferencing =
		std::is_convertible<add_ptr<std::tuple_element_t<J, Params>>, add_ptr<ZDeletableMark>>;

	template<class Z, class Params, typename std::enable_if<needs_dereferencing<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params)
	{
		return *std::get<I>(params);
	}

	template<class Z, class Params, typename std::enable_if<!needs_dereferencing<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params)
	{
		return std::get<I>(params);
	}
};

template<> struct ParamTraits<-1>
{
	template<class Z, class Params>
	auto operator()(Z z, const Params&)
	{
		return z;
	}
};

struct OnceMoveChecker
{
	OnceMoveChecker() { }
	OnceMoveChecker(OnceMoveChecker&&) noexcept { }
	std::atomic<uint32_t>& counter() { return m_counter; }
	std::atomic<uint32_t> m_counter;
};
template<class Z, class F, class P, int... I>
struct ZDeleter : OnceMoveChecker
{
	static const int magic = NEXT_MAGIC();
	Z	object;
	F	routine;
	P	params;
	ZDeleter(F ptr, P params)
		: object	(VK_NULL_HANDLE)
		, routine	(ptr)
		, params	(params) { }
	void operator()(Z* z) const
	{
		ASSERTION(getMagic() == z);
		const_cast<add_ptr<ZDeleter>>(this)->replace(VK_NULL_HANDLE);
	}
	void replace(const Z value)
	{
		if ((VK_NULL_HANDLE != object) && routine)
		{
			// Not neccessary but often prevents a flow to step into
			std::invoke(routine, ParamTraits<I>()(object, params)...);
		}
		object = value;
	}
	static Z* getMagic ()
	{
		union {
			Z*	p;
			int	x;
		} u {};
		u.x = magic;
		return u.p;
	}
};

template<class Z, class F, class P, class Indexes> struct ZDeleterTrait;
template<class Z, class F, class P, int... I>
struct ZDeleterTrait<Z, F, P, std::integer_sequence<int, I...>>
{
	typedef ZDeleter<Z, F, P, I...> type;
};

template<class Z, class F, F Ptr, class Tr, class... X> struct ZDeletable : protected std::shared_ptr<Z>, ZDeletableMark
{
	static_assert(std::is_pointer<Z>::value, "Z must be of pointer type");

	typedef Z vulkan_type;
	typedef std::tuple<X...> Params;
	typedef typename ZDeleterTrait<Z, F, Params, Tr>::type MyDeleter;
	ZDeletable(X... x) : std::shared_ptr<Z>(
							 MyDeleter::getMagic()
							 , MyDeleter(Ptr, std::make_tuple(x...)))
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
	}
	static ZDeletable create(Z z, X... x)
	{
		ZDeletable result(x...);
		auto del = std::get_deleter<MyDeleter>(result);
		ASSERTION(nullptr != del);
		del->object = z;
		return result;
	}
	static ZDeletable createEmpty(X... x)
	{
		ZDeletable result(x...);
		auto del = std::get_deleter<MyDeleter>(result);
		del->object = VK_NULL_HANDLE;
		return result;
	}
	ZAccess<Z> setter()
	{
		return ZAccess<Z>(true, std::bind(&ZDeletable::replace, this, std::placeholders::_1));
	}	
	bool empty () const
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return VK_NULL_HANDLE == del->object;
	}
	const Z* ptr() const
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		Z r = del->object;
		ASSERTION(r != VK_NULL_HANDLE);
		return &del->object;
	}
	Z operator*() const
	{
		return *ptr();
	}
	ZDeletable& operator=(Z z)
	{
		replace(z);
		return *this;
	}
	bool operator==(const ZDeletable& other) const
	{
		auto left_del = std::get_deleter<MyDeleter>(*this);
		auto right_del = std::get_deleter<MyDeleter>(other);
		ASSERTION(nullptr != left_del && nullptr != right_del);
		return (left_del->object == right_del->object);
	}
	void replace(Z z)
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		del->replace(z);
	}
	void reset()
	{
		replace(VK_NULL_HANDLE);
	}
	template<class T> T getParam() const
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return std::get<T>(del->params);
	}
	template<class T> add_ref<T> getParamRef()
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return std::get<T>(del->params);
	}
	template<class T> add_cref<T> getParamRef() const
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return std::get<T>(del->params);
	}
	template<class T> void setParam(add_cref<T> param)
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		std::get<T>(del->params) = param;
	}
	template<uint32_t At> auto getParam() const
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return std::get<At>(del->params);
	}
	long use_count() const noexcept
	{
		return std::shared_ptr<Z>::use_count();
	}
	std::atomic<uint32_t>& counter()
	{
		auto del = std::get_deleter<MyDeleter>(*this);
		ASSERTION(nullptr != del);
		return del->counter();
	}
};

typedef std::integer_sequence<int, -1>			swizzle_no_param;
typedef std::integer_sequence<int, -1, 0>		swizzle_one_param;
typedef std::integer_sequence<int, 0, -1, 1>	swizzle_two_params;

enum ZDistName
{
	None,
	RequiredLayers,				AvailableLayers,
	RequiredLayerExtensions,	AvailableLayerExtensions,
	RequiredDeviceExtensions,	AvailableDeviceExtensions
};
template<ZDistName, class CType_>
struct ZDistType
{
	CType_ data;
	ZDistType () : data() {}
	ZDistType (const CType_& v) : data(v) {}
};

typedef add_ptr<VkAllocationCallbacks> VkAllocationCallbacksPtr;

typedef ZDeletable<VkInstance,
	decltype(&vkDestroyInstance), &vkDestroyInstance,
	swizzle_one_param, VkAllocationCallbacksPtr, /*apiVersion*/ uint32_t,
	ZDistType<RequiredLayers, std::vector<std::string>>,
	ZDistType<AvailableLayerExtensions, std::vector<std::string>>>					ZInstance;

typedef ZDeletable<VkPhysicalDevice,
	void(*)(void), nullptr,
	std::integer_sequence<int>,
	VkAllocationCallbacksPtr,
	ZInstance,
	uint32_t, /*Physical device index across the system*/
	std::vector<std::string> /*extensions*/>					ZPhysicalDevice;

typedef ZDeletable<VkQueue,
	void(*)(void), nullptr,
	std::integer_sequence<int>,
	uint32_t /*queueFamilyIndex*/>								ZQueue;

enum class ZQueueType { Graphics, Compute, Transfer, Present };
typedef std::map<ZQueueType, std::optional<ZQueue> >			ZQueueInfos;
typedef ZDeletable<VkDevice,
	decltype(&vkDestroyDevice), &vkDestroyDevice,
	swizzle_one_param, VkAllocationCallbacksPtr,
	ZPhysicalDevice, ZQueueInfos>								ZDevice;

struct GLFWwindow;
void freeWindow(add_ptr<GLFWwindow>);
typedef typename std::add_pointer<GLFWwindow>::type GLFWwindowPtr;
typedef ZDeletable<GLFWwindowPtr,
	decltype(&freeWindow), &freeWindow,
	swizzle_no_param>											ZGLFWwindowPtr;

typedef ZDeletable<VkSurfaceKHR,
	decltype(&vkDestroySurfaceKHR), &vkDestroySurfaceKHR,
	swizzle_two_params, ZInstance, VkAllocationCallbacksPtr,
	ZGLFWwindowPtr>												ZSurfaceKHR;

typedef ZDeletable<VkSwapchainKHR,
	decltype(&vkDestroySwapchainKHR),	&vkDestroySwapchainKHR,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZSwapchainKHR;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZUnboundImageView;

typedef ZDeletable<VkFramebuffer,
	decltype(&vkDestroyFramebuffer), &vkDestroyFramebuffer,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZFramebuffer;

typedef ZDeletable<VkRenderPass,
	decltype(&vkDestroyRenderPass), &vkDestroyRenderPass,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	uint32_t, /*attachment count*/
	std::vector<VkClearValue>,
	bool, /*enable depth attachment*/
	VkFormat /*depth format*/>									ZRenderPass;

typedef ZDeletable<VkShaderModule,
	decltype(&vkDestroyShaderModule), &vkDestroyShaderModule,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZShaderModule;

typedef ZDeletable<VkCommandPool,
	decltype(&vkDestroyCommandPool), &vkDestroyCommandPool,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	ZQueue>														ZCommandPool;

inline void freeCommandBuffer (VkDevice device, VkCommandBuffer buff, VkCommandPool pool) {
	vkFreeCommandBuffers(device, pool, 1, &buff); }
typedef ZDeletable<VkCommandBuffer,
	decltype(&freeCommandBuffer), &freeCommandBuffer,
	swizzle_two_params, ZDevice, ZCommandPool>					ZCommandBuffer;

// ZCommandBuffer <- ZCommandPool <- ZQueue <- queueFamilyIndex

typedef ZDeletable<VkFence,
	decltype(&vkDestroyFence), &vkDestroyFence,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZFence;

typedef ZDeletable<VkSemaphore,
	decltype(&vkDestroySemaphore), &vkDestroySemaphore,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZSemaphore;

typedef ZDeletable<VkDeviceMemory,
	decltype(&vkFreeMemory), &vkFreeMemory,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	VkMemoryPropertyFlags, VkDeviceSize, uint8_t*>				ZDeviceMemory;

typedef ZDeletable<VkImage,
	decltype(&vkDestroyImage), &vkDestroyImage,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	VkImageCreateInfo, ZDeviceMemory, VkDeviceSize>				ZImage;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	VkImageViewCreateInfo, ZImage>								ZImageView;

typedef ZDeletable<VkSampler,
	decltype(&vkDestroySampler), &vkDestroySampler,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	VkSamplerCreateInfo>										ZSampler;

typedef ZDeletable<VkBuffer,
	decltype(&vkDestroyBuffer), &vkDestroyBuffer,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	VkBufferCreateInfo, ZDeviceMemory, VkDeviceSize>			ZBuffer;

typedef ZDeletable<VkDescriptorPool,
	decltype(&vkDestroyDescriptorPool), &vkDestroyDescriptorPool,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr>		ZDescriptorPool;

inline void freeDescriptorSet (VkDevice device, VkDescriptorSet set, VkDescriptorPool pool) {
	vkFreeDescriptorSets(device, pool, 1, &set); }
typedef ZDeletable<VkDescriptorSet,
	decltype(&freeDescriptorSet), &freeDescriptorSet,
	swizzle_two_params, ZDevice, ZDescriptorPool>				ZDescriptorSet;

typedef ZDeletable<VkDescriptorSetLayout,
	decltype(&vkDestroyDescriptorSetLayout), &vkDestroyDescriptorSetLayout,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	std::optional<ZDescriptorSet>>								ZDescriptorSetLayout;

typedef ZDeletable<VkPipelineLayout,
	decltype(&vkDestroyPipelineLayout), &vkDestroyPipelineLayout,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	std::vector<ZDescriptorSetLayout>,
	VkPushConstantRange, std::type_index>						ZPipelineLayout;

typedef ZDeletable<VkPipeline,
	decltype(&vkDestroyPipeline), &vkDestroyPipeline,
	swizzle_two_params, ZDevice, VkAllocationCallbacksPtr,
	ZPipelineLayout, VkPipelineBindPoint>						ZPipeline;

#endif // __VTF_ZDELETABLE_HPP_INCLUDED__
 
