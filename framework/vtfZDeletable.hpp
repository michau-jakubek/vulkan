#ifndef __VTF_ZDELETABLE_HPP_INCLUDED__
#define __VTF_ZDELETABLE_HPP_INCLUDED__

#include <atomic>
#include <bitset>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>
#include <utility>

#include "vulkan/vulkan.h"
#include "vtfZInstanceDeviceInterface.hpp"
#include "vtfThreadSafeLogger.hpp"
#include "vtfStrictTemplates.hpp"
#include "demangle.hpp"

#define NEXT_MAGIC() (__COUNTER__ + 1)

template<class C> using add_cst    = typename std::add_const<C>::type;
template<class P> using add_ptr    = typename std::add_pointer<P>::type;
template<class R> using add_ref    = typename std::add_lvalue_reference<R>::type;
template<class R> using add_rref   = typename std::add_rvalue_reference<R>::type;
template<class P> using add_cptr   = add_ptr<add_cst<P>>;
template<class R> using add_cref   = add_ref<add_cst<R>>;
using void_cptr = add_cptr<void>;
using void_ptr = add_ptr<void>;

template<class X> struct add_extent	{ typedef X type[]; };

// glslang	9a415ad3c69eae445b84ee9647c10e46d42dc25d
// headers	6782ffb1a16d3485d6f17ca919729d8a9b75d990
// tools	c4dd14909e60c3b2d0be035d84e232496cb2e7ab
// docs	0a10fc1caae9a95de0fb304ca4a778fbd7936ba0


VkAllocationCallbacks* getAllocationCallbacks();
void deletable_selfTest ();

void writeBackTrace (add_ref<std::ostringstream> ss);
void writeExpression (add_ref<std::ostringstream> ss,
					  const char* func, const char* file, int line, const char* expr, VkResult res, int ind);
template<typename... Args>
void assertion (
	bool		cond,
	const char*	func,
	const char*	file,
	int			line,
	const char*	expr,
	VkResult	res,
	bool		print,
	Args&&...	args)
{
	(void)cond;
	const int ind = 2;
	std::ostringstream ss;
	writeExpression(ss, func, file, line, expr, res, ind);
	if (print)
	{
		ss << std::string(ind, ' ');
		((ss << args), ...);
		ss << std::endl;
	}
	writeBackTrace(ss);
	ss.flush();
	throw std::runtime_error(ss.str());
}

bool	backtraceEnabled			();
void	backtraceEnabled			(bool enable);

void*	alloc_allocate				(size_t numBytes);
void	alloc_deallocate			(void* p, size_t numBytes);
size_t	alloc_get_allocation_count	();
size_t	alloc_get_allocation_size	();

#ifndef STRINGIZE
#define STRINGIZE(expr__) #expr__
#endif

#define ASSERT_IMPL(cond__, sexpr__, vkrez__, print__, ...) \
	assertion( cond__, __func__, __FILE__, __LINE__, sexpr__, vkrez__, print__, __VA_ARGS__)

#define ASSERTFALSE(...) ASSERT_IMPL(false, nullptr, VK_SUCCESS, true, __VA_ARGS__)

#define ASSERTION(expr__) { \
	const bool rez__ = !!(expr__); if (false == rez__) \
	ASSERT_IMPL( !!(expr__), STRINGIZE(expr__), VK_SUCCESS, false, 0); }

#define ASSERTMSG(expr__, ...) { \
	const bool rez__ = !!(expr__); if (false == rez__) \
	ASSERT_IMPL( rez__, STRINGIZE(expr__), VK_SUCCESS, true, __VA_ARGS__); }

#define ASSERT_NOT_IMPLEMENTED() ASSERTFALSE("Not implemented yet")

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
template<class X> constexpr bool is_zdeletable2 = std::is_convertible_v<add_ptr<X>, add_ptr<ZDeletableMark>>;

constexpr int DontDereferenceParamOffset = 100;

template<int I> struct ParamTraits
{
	template<std::size_t J> struct Squash {
		enum { K = J < DontDereferenceParamOffset ? J : (J - DontDereferenceParamOffset) };
	};

	// needs a dereferencing
	template<std::size_t J, class Params> using is_zdeletable =
		std::is_convertible<add_ptr<std::tuple_element_t<Squash<J>::K, Params>>, add_ptr<ZDeletableMark>>;

	template<class Z, class Params, typename std::enable_if<
		(I < DontDereferenceParamOffset) && is_zdeletable<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params) {
		return *std::get<Squash<I>::K>(params);
	}

	template<class Z, class Params, typename std::enable_if<
		(I < DontDereferenceParamOffset) && ! is_zdeletable<I, Params>::value, bool>::type = false>
	auto operator()(Z, const Params& params) {
		return std::get<Squash<I>::K>(params);
	}

	template<class Z, class Params, typename std::enable_if<
		is_zdeletable<I, Params>::value && !(I < DontDereferenceParamOffset), bool>::type = false>
	auto operator()(Z, const Params& params) {
		return std::get<Squash<I>::K>(params);
	}
};

template<> struct ParamTraits<-1>
{
	template<class Z, class Params>
	Z operator()(Z z, const Params&) {
		return z;
	}
};

template<> struct ParamTraits<-2>
{
	template<class Z, class Params>
	const Params& operator()(Z, const Params& params) {
		return params;
	}
};

template<class Object, class D, int... I>
struct ZObjectDeleter
{
	virtual ~ZObjectDeleter () = default;
	void operator() (Object* obj) const
	{
		obj->template destroy<D, I...>();
		delete obj;
	}
};

template<class Object, class D, class Indexes> struct ZDeleterTraits;
template<class Object, class D, int... I>
	struct ZDeleterTraits<Object, D, std::integer_sequence<int, I...>>
	{
		typedef ZObjectDeleter<Object, D, I...> type;
		virtual ~ZDeleterTraits () = default;
	};

template<class Z, class F, class P>
struct ZObject
{
	Z			handle;
	F			routine;
	P			params;
	std::string	name;
	bool		verbose;
	ZObject (F ptr)
		: handle	(VK_NULL_HANDLE)
		, routine	(ptr)
		, params	()
		, name		()
		, verbose	(false) { }
	ZObject (Z z, F ptr, add_cref<P> params)
		: handle	(z)
		, routine	(ptr)
		, params	(params)
		, name		()
		, verbose	(false) { }
	virtual ~ZObject () = default;
	template<class D, int... I> void destroy() {
		if (handle != VK_NULL_HANDLE && routine != nullptr) {
			std::invoke(routine, ParamTraits<I>()(handle, params)...);
			if (verbose) std::cout << "[INFO] Destroing: " << demangledName<Z>() << ' ' << handle << std::endl;
			handle = VK_NULL_HANDLE;
		}
	}
};

struct ZDeletableBase { };
template<class Z, class F, F Ptr, class Tr, class Inh, class... X> struct ZDeletable
	: protected std::shared_ptr<ZObject<Z, F, std::tuple<X...>>>, public Inh, public ZDeletableMark
{
	static_assert(std::is_pointer<Z>::value, "Z must be of pointer type");

	typedef Z											handle_type;
	typedef ZDeletable<Z,F,Ptr,Tr,Inh,X...>				deletable_type;
	typedef ZObject<Z,F,std::tuple<X...>>				AnObject;
	typedef std::shared_ptr<AnObject>					super;
	typedef typename ZDeleterTraits<AnObject, deletable_type, Tr>::type	ADeleter;
	ZDeletable()
		: super(new AnObject(Ptr), ADeleter())
		, ZDeletableMark() { /* Intentionally empty */ }
	ZDeletable(Z z, add_cref<X>... x)
		: super(new AnObject(z, Ptr, std::make_tuple(x...)), ADeleter())
		, ZDeletableMark() { /* Intentionally empty */ }
	ZDeletable(const super& s)
		: super(s)
		, ZDeletableMark() { /* Intentionally empty */ }
	virtual ~ZDeletable() = default;
	static typename ZDeletable::deletable_type create(Z z, add_cref<X>... x)
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
	Z handle () const
	{
		return super::get()->handle;
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
	super asSharedPtr () const
	{
		return (*this);
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
	template<class Then, class Else> auto select(const Then& then_, const Else& else_)
		-> std::common_type_t<::vtf::lambda_result_type<Then>, ::vtf::lambda_result_type<Else>>
	{
		auto predicate = [this]{ return has_handle(); };
		static_assert(::vtf::is_lambda_param_v(predicate), "???");
		return ::vtf::select(predicate, then_, else_);
	}
	template<class MaybeLambda>
		ZDeletable operator| (MaybeLambda&& maybeLambda)
	{
		return ::vtf::select(has_handle(), *this, std::forward<MaybeLambda>(maybeLambda));
	}
	long use_count() const noexcept	{ return super::use_count(); }
	add_cref<std::string> name () const	{ return super::get()->name; }
	void name (add_cref<std::string> newName) { super::get()->name = newName; }
	bool verbose () const { return super::get()->verbose; }
	void verbose (bool value) { super::get()->verbose = value; }
	friend std::ostream& operator<<(std::ostream& str, add_cref<ZDeletable> z)
	{
		if (z.name().length() == 0u)
			return (str << demangledName<handle_type>() << ' ' << z.handle());
		return (str << demangledName<handle_type>() << ' ' << z.handle() << std::quoted(z.name()));
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
 * (-2) means that pointer to ZDeletable being deleted
 * <0, DontDereferenceParamOffset) means that
 *    a parameter at index will be derefernced if it is ZDeletable
 *    otherwise will be passed as is
 * <DontDereferenceParamOffset, ...) means that a parameter will be passed as is
 *    and it will not be dereferenced even if it is ZDeletable
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
	DesiredRequiredDeviceExtensions,	AvailableDeviceExtensions,
	Width, Height, Depth,		PatchControlPoints, SubpassIndex,
	SizeFirst, SizeSecond, SizeThird,
	VtfVer, ApiVer, VulkanVer, SpirvVer,
	QueueFamilyIndex, QueueIndex, QueueFlags,
	CullModeFlags, DepthTestEnable, DepthWriteEnable, StencilTestEnable,
	LineWidth, AttachmentCount, SubpassCount, ViewportCount, ScissorCount,
	SpecConstants, BlendAttachmentState, BlendConstants,
};
template<ZDistName, class CType_>
struct ZDistType
{
	ZDistType () : data() {}
	ZDistType (const CType_& v) : data(v)	{}
	operator add_cref<CType_>	() const	{ return data; }
	add_cref<CType_> operator ()() const	{ return data; }
	add_cref<CType_> get		() const	{ return data; }
	operator add_ref<CType_>	()			{ return data; }
	add_ref<CType_> operator ()	()			{ return data; }
	add_ref<CType_> get			()			{ return data; }
private:
	CType_ data;
};

typedef add_ptr<VkAllocationCallbacks> VkAllocationCallbacksPtr;

void vtfDestroyInstance (VkInstance, VkAllocationCallbacksPtr);
typedef ZDeletable<VkInstance,
	decltype(&vtfDestroyInstance), &vtfDestroyInstance,
	swizzle_two_param, vtf::ZInstanceSingleton,
	VkAllocationCallbacksPtr, std::string, /*apiVersion*/ uint32_t,
	ZDistType<AvailableLayers, std::vector<std::string>>,
	ZDistType<RequiredLayers, std::vector<std::string>>,
	ZDistType<AvailableLayerExtensions, std::vector<std::string>>,
	ZDistType<RequiredLayerExtensions, std::vector<std::string>>,
	vtf::Logger, VkDebugUtilsMessengerEXT, VkDebugReportCallbackEXT>
ZInstance;

typedef ZDeletable<VkPhysicalDevice,
	void(*)(void), nullptr
	, swizzle_no_param
	, ZDeletableBase
	, VkAllocationCallbacksPtr
	, ZInstance
	, uint32_t /*Physical device index across the system*/
	, ZDistType<AvailableDeviceExtensions, std::vector<std::string>>
	, ZDistType<DesiredRequiredDeviceExtensions, std::vector<std::string>>
	, VkPhysicalDeviceProperties>
ZPhysicalDevice;

struct GLFWwindow;
void freeWindow(add_ptr<GLFWwindow>);
typedef typename std::add_pointer<GLFWwindow>::type GLFWwindowPtr;
typedef ZDeletable<GLFWwindowPtr,
	decltype(&freeWindow), &freeWindow,
	swizzle_one_param, ZDeletableBase>
ZGLFWwindowPtr;

typedef ZDeletable<VkSurfaceKHR,
	decltype(&vkDestroySurfaceKHR), &vkDestroySurfaceKHR,
	swizzle_three_params, ZDeletableBase,
	ZInstance, VkAllocationCallbacksPtr,
	std::weak_ptr<ZGLFWwindowPtr::AnObject>>
ZSurfaceKHR;

struct ZDeviceQueueCreateInfo : VkDeviceQueueCreateInfo
{
	std::bitset<32>	queues;
	VkQueueFlags	queueFlags;
	bool			surfaceSupport;
	ZDeviceQueueCreateInfo () = default;
};
void vtfDestroyDevice (VkDevice, VkAllocationCallbacksPtr, add_cref<ZPhysicalDevice>);
typedef ZDeletable<VkDevice,
	decltype(&vtfDestroyDevice), &vtfDestroyDevice,
	std::integer_sequence<int, -1, 0, (DontDereferenceParamOffset + 1)>,
	vtf::ZDeviceSingleton,
	VkAllocationCallbacksPtr,
	ZPhysicalDevice,
	std::vector<ZDeviceQueueCreateInfo>
> ZDevice;

typedef std::tuple<	ZDistType<QueueFamilyIndex, uint32_t>
					, ZDistType<QueueIndex, uint32_t>
					, ZDistType<QueueFlags, VkQueueFlags>
					, ZDevice /*device which this queue belong*/
					, bool /*surfaceSupported*/
> QueueParams;
// implemented in vtfZUtils.cpp
void releaseQueue(const QueueParams&);
typedef ZDeletable<VkQueue,
	decltype(&releaseQueue), &releaseQueue
	, std::integer_sequence<int, -2>
	, ZDeletableBase
	, ZDistType<QueueFamilyIndex, uint32_t>
	, ZDistType<QueueIndex, uint32_t>
	, ZDistType<QueueFlags, VkQueueFlags>
	, ZDevice /*device which this queue belong*/
	, bool /*surfaceSupported*/
> ZQueue;

typedef ZDeletable<VkSwapchainKHR,
	decltype(&vkDestroySwapchainKHR),	&vkDestroySwapchainKHR,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZSwapchainKHR;

typedef ZDeletable<VkShaderModule,
	decltype(&vkDestroyShaderModule), &vkDestroyShaderModule,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkShaderStageFlagBits, std::string>
ZShaderModule;

typedef ZDeletable<VkCommandPool,
	decltype(&vkDestroyCommandPool), &vkDestroyCommandPool,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZQueue>
ZCommandPool;

inline void freeCommandBuffer (VkDevice device, VkCommandBuffer buff, VkCommandPool pool) {
	vkFreeCommandBuffers(device, pool, 1, &buff); }
typedef ZDeletable<VkCommandBuffer,
	decltype(&freeCommandBuffer), &freeCommandBuffer,
	swizzle_three_params, ZDeletableBase, ZDevice, ZCommandPool, bool>
ZCommandBuffer;

typedef ZDeletable<VkFence,
	decltype(&vkDestroyFence), &vkDestroyFence,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZFence;

typedef ZDeletable<VkSemaphore,
	decltype(&vkDestroySemaphore), &vkDestroySemaphore,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZSemaphore;

typedef ZDeletable<VkDeviceMemory,
	decltype(&vkFreeMemory), &vkFreeMemory,
	swizzle_three_params,
	ZDeletableBase,
	ZDevice,
	VkAllocationCallbacksPtr,
	VkMemoryPropertyFlags, 
	VkDeviceSize,							// VkMemoryRequirements::size
	ZDistType<SizeSecond, VkDeviceSize>,	// requested size (buffer|image)
	add_ptr<uint8_t>>
ZDeviceMemory;

typedef ZDeletable<VkImage,
	decltype(&vkDestroyImage), &vkDestroyImage,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkImageCreateInfo, ZDeviceMemory, VkDeviceSize>
ZImage;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkImageViewCreateInfo, ZImage>
ZImageView;

typedef ZDeletable<VkImageView,
	decltype(&vkDestroyImageView), &vkDestroyImageView,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZUnboundImageView;

typedef ZDeletable<VkRenderPass,
	decltype(&vkDestroyRenderPass), &vkDestroyRenderPass,
	swizzle_three_params,
	ZDeletableBase,
	ZDevice,
	VkAllocationCallbacksPtr,
	ZDistType<AttachmentCount, uint32_t>,
	ZDistType<SubpassCount, uint32_t>,
	std::vector<VkClearValue>,
	bool, /*enable depth attachment*/
	VkImageLayout /*finalLayout*/>
ZRenderPass;

typedef ZDeletable<VkFramebuffer,
	decltype(&vkDestroyFramebuffer), &vkDestroyFramebuffer,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<Width, uint32_t>, ZDistType<Height, uint32_t>,
	ZRenderPass, std::vector<ZImageView>>
ZFramebuffer;

typedef ZDeletable<VkSampler,
	decltype(&vkDestroySampler), &vkDestroySampler,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkSamplerCreateInfo>
ZSampler;

struct type_index_with_default : public std::type_index
{
	type_index_with_default () : std::type_index(typeid(void)) {}
	type_index_with_default (add_cref<std::type_info> info) : std::type_index(info) {}
	type_index_with_default (add_cref<std::type_index> index) : std::type_index(index) {}
	template<class X> static type_index_with_default make () {
		return type_index_with_default(typeid(X));
	}
};

typedef ZDeletable<VkBuffer,
	decltype(&vkDestroyBuffer), &vkDestroyBuffer,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkBufferCreateInfo, std::vector<ZDeviceMemory>, VkDeviceSize,
	type_index_with_default, VkExtent3D, VkFormat>
ZBuffer;

typedef ZDeletable<VkDescriptorPool,
	decltype(&vkDestroyDescriptorPool), &vkDestroyDescriptorPool,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZDescriptorPool;

inline void freeDescriptorSet (VkDevice device, VkDescriptorSet set, VkDescriptorPool pool) {
	vkFreeDescriptorSets(device, pool, 1, &set); }
typedef ZDeletable<VkDescriptorSet,
	decltype(&freeDescriptorSet), &freeDescriptorSet,
	swizzle_three_params, ZDeletableBase, ZDevice, ZDescriptorPool>
ZDescriptorSet;

typedef ZDeletable<VkDescriptorSetLayout,
	decltype(&vkDestroyDescriptorSetLayout), &vkDestroyDescriptorSetLayout,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZDescriptorSet>
ZDescriptorSetLayout;

typedef ZDeletable<VkPipelineLayout,
	decltype(&vkDestroyPipelineLayout), &vkDestroyPipelineLayout,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	std::vector<ZDescriptorSetLayout>,
	std::vector<VkPushConstantRange>,
	std::vector<type_index_with_default>>
ZPipelineLayout;

typedef ZDeletable<VkPipeline,
	decltype(&vkDestroyPipeline), &vkDestroyPipeline,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZPipelineLayout, ZRenderPass, VkPipelineBindPoint>
ZPipeline;

#endif // __VTF_ZDELETABLE_HPP_INCLUDED__
 
