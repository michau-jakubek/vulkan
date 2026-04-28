#ifndef __VTF_ZDELETABLE_HPP_INCLUDED__
#define __VTF_ZDELETABLE_HPP_INCLUDED__

#include <any>
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
#include "vtfProgressRecorder.hpp"
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
using void_pfn = void(*)(void);

template<class X> struct add_extent	{ typedef X type[]; };

VkAllocationCallbacks* getAllocationCallbacks();
void deletable_selfTest ();

void throwCorrectException (VkResult res, add_cref<std::string> msg);
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
	std::ostringstream ss;
	const std::string::size_type ind = 2;
	writeExpression(ss, func, file, line, expr, res, ind);
	if (print)
	{
		ss << std::string(ind, ' ');
		((ss << args), ...);
		ss << std::endl;
	}
	writeBackTrace(ss);
	ss.flush();
	throwCorrectException(res, ss.str());
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
	typedef std::function<void(Z, add_cref<std::string>)> Callback;
	Z			object;
	bool		setOrGet;
	std::string	message;
	Callback	callback;
	ZAccess(bool setOrGet, add_cref<std::string> msg, Callback cb)
		: object	(VK_NULL_HANDLE)
		, setOrGet	(setOrGet)
		, message	(msg)
		, callback	(cb)
	{
	}
	~ZAccess() { if (setOrGet) callback(object, message); }
	operator Z*() { return &object; }
};

struct ZDeletableMark { };
constexpr int DontDereferenceParamOffset = 100;

template<int I> struct ParamTraits1
{
	template<int J> struct Squash {
		enum {
			K = (J <= -DontDereferenceParamOffset)
				? -(J + DontDereferenceParamOffset)
				: (J >= 0 && J < DontDereferenceParamOffset)
					? J
					: (J - DontDereferenceParamOffset)
		};
	};

	// needs a dereferencing
	template<int J, class Params> using is_zdeletable =
		std::is_convertible<add_ptr<std::tuple_element_t<Squash<J>::K, Params>>, add_ptr<ZDeletableMark>>;

	template<class Z, class Params, typename std::enable_if<
		(I < DontDereferenceParamOffset) && is_zdeletable<I, Params>::value, bool>::type = true>
	auto operator()(Z, const Params& params) {
		return *std::get<Squash<I>::K>(params);
	}

	template<class Z, class Params, typename std::enable_if<
		is_zdeletable<I, Params>::value && (I >= DontDereferenceParamOffset), bool>::type = false>
	auto operator()(Z, const Params& params) {
		return std::get<Squash<I>::K>(params);
	}

	template<class Z, class Params, typename std::enable_if<
		(I < DontDereferenceParamOffset) && !is_zdeletable<I, Params>::value, bool>::type = false>
	auto operator()(Z, const Params& params) {
		return std::get<Squash<I>::K>(params);
	}
};

template<> struct ParamTraits1<-1>
{
	template<class Z, class Params>
	Z operator()(Z z, const Params&) {
		return z;
	}
};

template<> struct ParamTraits1<-2>
{
	template<class Z, class Params>
	const Params& operator()(Z, const Params& params) {
		return params;
	}
};

template<int I> struct ParamTraits2
{
	template<class Z, class Params>
	auto operator()(Z, const Params& params) {
		return &std::get<ParamTraits1<0>::Squash<I>::K>(params);
	}
};

template<int I> struct ParamTraits
{
	typedef typename std::conditional<
		(I <= -DontDereferenceParamOffset), ParamTraits2<I>, ParamTraits1<I>>::type type;
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

struct OnZDeletableDestroing
{
    typedef void (*PFN)(void_ptr pZDeletable);
    const PFN onDestroing;
    OnZDeletableDestroing() : onDestroing(nullptr) {}
    OnZDeletableDestroing(const PFN onDestroing_) : onDestroing(onDestroing_) {}
    void operator()(void_ptr pZDeletable) const {
        if (onDestroing) (*onDestroing)(pZDeletable);
    };
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
			if (verbose) std::cout << "[INFO] Destroing: " << name << ", " << demangledName<Z>() << ' ' << handle << std::endl;
			std::invoke(routine, typename ParamTraits<I>::type()(handle, params)...);
			handle = VK_NULL_HANDLE;
		}
	}
};

struct ZDeletableBase { virtual ~ZDeletableBase() = default; };
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
    virtual ~ZDeletable()
    {
        if constexpr ((std::is_same<OnZDeletableDestroing, X>::value || ...)) {
            getParam<OnZDeletableDestroing>()(this);
        }
    }
	static typename ZDeletable::deletable_type create(Z z, add_cref<X>... x)
	{
		return ZDeletable(z, x...);
	}
	ZAccess<Z> setter(add_cref<std::string> msg = std::string())
	{
		return ZAccess<Z>(true, msg, std::bind(&ZDeletable::replace, this, std::placeholders::_1, std::placeholders::_2));
	}
	bool has_handle () const
	{
		add_cptr<AnObject> obj = super::get();
		ASSERTMSG(nullptr != obj, "Object must not be null");
		return (VK_NULL_HANDLE != obj->handle);
	}
	explicit operator bool () const
	{
		return has_handle();
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
		ASSERTMSG(VK_NULL_HANDLE != *p, "Vulkan handle must not be VK_NULL_HANDLE (", demangledName<Z>(), ")");
		return p;
	}
	std::pair<add_cptr<AnObject>, size_t> object () const
	{
		return std::pair<add_cptr<AnObject>, size_t>(super::get(), sizeof(AnObject));
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
	template<class T> add_ref<ZDeletable> setParam(add_cref<T> param)
	{
		std::get<T>(super::get()->params) = param;
		return *this;
	}
	template<class T> add_ref<ZDeletable> forwardParam(add_rref<T> param)
	{
		std::get<T>(super::get()->params) = std::forward<T>(param);
		return *this;
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
	void replace(Z z, add_cref<std::string> msg)
	{
		ASSERTMSG(VK_NULL_HANDLE != z, demangledName<Z>(), " must not be null ", msg);
		add_ptr<AnObject> obj = super::get();
		ASSERTION(VK_NULL_HANDLE == obj->handle);
		obj->handle = z;
	}
};

/*
 * Controls the params passed to a deleter routine from a tuple.
 * (-1) means that Vulkan handle from ZDeletable
 * (-2) means that const reference to the tuple ZObject::params part
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
typedef std::integer_sequence<int, -DontDereferenceParamOffset, 0, -1, 1>	swizzle_four_params;

enum ZDistName
{
	None, Undeletable,
	RequiredLayers,				AvailableLayers,
	RequiredLayerExtensions,	AvailableLayerExtensions,
	RequiredDeviceExtensions,	AvailableDeviceExtensions,
	Width, Height, Depth, ViewMask, PatchControlPoints, SubpassIndex,
	LayoutIdentifier, PrimitiveRestart,
	Count, SizeFirst, SizeSecond, SizeThird,
	SomeZero, SomeOne, SomeTwo, SomeThree,
	VtfVer, ApiVer, VulkanVer, SpirvVer,
	QueueFamilyIndex, QueueIndex, QueueFlags,
	CullModeFlags, DepthTestEnable, DepthWriteEnable, StencilTestEnable,
	DepthMinBounds, DepthMaxBounds,
	LineWidth, AttachmentCount, SubpassCount, ViewportCount, ScissorCount,
	SpecConstants, BlendAttachmentState, BlendConstants, PipelineCreateFlags,
	RenderingAttachmentLocations, RenderingInpuAttachmentIndices, RasterizerDiscardEnable,
};
template<ZDistName name, typename CType_>
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

void vtfDestroyInstance (VkInstance, VkAllocationCallbacksPtr, uint32_t);
typedef ZDeletable<VkInstance,
	decltype(&vtfDestroyInstance), &vtfDestroyInstance,
	std::integer_sequence<int, -1, 0, 1>, vtf::ZInstanceSingleton,
	VkAllocationCallbacksPtr,
	ZDistType<Undeletable, uint32_t>,
	/*appName*/ std::string, /*apiVersion*/ uint32_t,
	ZDistType<AvailableLayers, std::vector<std::string>>,
	ZDistType<RequiredLayers, std::vector<std::string>>,
	ZDistType<AvailableLayerExtensions, std::vector<std::string>>,
	ZDistType<RequiredLayerExtensions, std::vector<std::string>>,
    vtf::Logger, vtf::ProgressRecorder,
    VkDebugUtilsMessengerEXT, VkDebugReportCallbackEXT,
    OnZDeletableDestroing
>
ZInstance;

typedef ZDeletable<VkPhysicalDevice,
	void_pfn, nullptr
	, swizzle_no_param
	, ZDeletableBase
	, VkAllocationCallbacksPtr
	, ZInstance
	, uint32_t /*Physical device index across the system*/
	, ZDistType<AvailableDeviceExtensions, std::vector<std::string>>
	, ZDistType<RequiredDeviceExtensions, std::vector<std::string>>
	, VkPhysicalDeviceProperties>
ZPhysicalDevice;

struct GLFWwindow;
void freeWindow(add_ptr<GLFWwindow>);
typedef typename std::add_pointer<GLFWwindow>::type GLFWwindowPtr;
typedef ZDeletable<GLFWwindowPtr,
	decltype(&freeWindow), &freeWindow,
	swizzle_one_param, ZDeletableBase>
ZGLFWwindowPtr;

void vtfDestroySurfaceKHR(void_cptr, VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
typedef ZDeletable<VkSurfaceKHR,
	decltype(&vtfDestroySurfaceKHR), &vtfDestroySurfaceKHR,
	swizzle_four_params, ZDeletableBase,
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
void vtfDestroyDevice (VkDevice, VkAllocationCallbacksPtr, uint32_t);
typedef ZDeletable<VkDevice,
	decltype(&vtfDestroyDevice), &vtfDestroyDevice,
	std::integer_sequence<int, -1, 0, 2>,
	vtf::ZDeviceSingleton,
	VkAllocationCallbacksPtr,
	ZPhysicalDevice,
	ZDistType<Undeletable, uint32_t>,
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
	, ZDevice /*device which this queue belongs*/
	, bool /*surfaceSupported*/
> ZQueue;

void vtfDestroySwapchainKHR(void_cptr, VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*);
typedef ZDeletable<VkSwapchainKHR,
	decltype(&vtfDestroySwapchainKHR),	&vtfDestroySwapchainKHR,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZSwapchainKHR;

void vtfDestroyShaderModule(void_cptr, VkDevice, VkShaderModule, const VkAllocationCallbacks*);
typedef ZDeletable<VkShaderModule,
	decltype(&vtfDestroyShaderModule), &vtfDestroyShaderModule,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkShaderStageFlagBits, std::string,
	// <Collection:id, SBTShaderGroup:index, pipelineGroupIndex, pipelineShaderIndex>
	std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>
>
ZShaderModule;

void vtfDestroyCommandPool(void_cptr, VkDevice, VkCommandPool, const VkAllocationCallbacks*);
typedef ZDeletable<VkCommandPool,
	decltype(&vtfDestroyCommandPool), &vtfDestroyCommandPool,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZQueue>
ZCommandPool;

void freeCommandBuffer(void_cptr, VkDevice, VkCommandBuffer, VkCommandPool);
typedef ZDeletable<VkCommandBuffer,
	decltype(&freeCommandBuffer), &freeCommandBuffer,
	swizzle_four_params, ZDeletableBase, ZDevice, ZCommandPool, bool>
ZCommandBuffer;

void vtfDestroyFence(void_cptr, VkDevice, VkFence, const VkAllocationCallbacks*);
typedef ZDeletable<VkFence,
	decltype(&vtfDestroyFence), &vtfDestroyFence,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZFence;

void vtfDestroySemaphore(void_cptr, VkDevice, VkSemaphore, const VkAllocationCallbacks*);
typedef ZDeletable<VkSemaphore,
	decltype(&vtfDestroySemaphore), &vtfDestroySemaphore,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZSemaphore;

void vtfFreeMemory(void_cptr, VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);
typedef ZDeletable<VkDeviceMemory,
	decltype(&vtfFreeMemory), &vtfFreeMemory,
	swizzle_four_params,
	ZDeletableBase,
	ZDevice,
	VkAllocationCallbacksPtr,
	VkMemoryPropertyFlags, 
	VkDeviceSize,							// VkMemoryRequirements::size
	ZDistType<SizeSecond, VkDeviceSize>,	// requested size (buffer|image)
	add_ptr<uint8_t>>
ZDeviceMemory;

void vtfDestroyImage(void_cptr, VkDevice, VkImage, const VkAllocationCallbacks*);
typedef ZDeletable<VkImage,
	decltype(&vtfDestroyImage), &vtfDestroyImage,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkImageCreateInfo, ZDeviceMemory, VkDeviceSize>
ZImage;

void vtfDestroyImageView(void_cptr, VkDevice, VkImageView, const VkAllocationCallbacks*);
typedef ZDeletable<VkImageView,
	decltype(&vtfDestroyImageView), &vtfDestroyImageView,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkImageViewCreateInfo, ZImage>
ZImageView;

typedef ZDeletable<VkImageView,
	decltype(&vtfDestroyImageView), &vtfDestroyImageView,
	swizzle_three_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZUnboundImageView;

void vtfDestroyRenderPass(void_cptr, VkDevice, VkRenderPass, const VkAllocationCallbacks*);
typedef ZDeletable<VkRenderPass,
	decltype(&vtfDestroyRenderPass), &vtfDestroyRenderPass
	, swizzle_four_params
	, ZDeletableBase
	, ZDevice
	, VkAllocationCallbacksPtr
	, int // version 1 or 2
	, ZDistType<AttachmentCount, uint32_t>
	, ZDistType<SubpassCount, uint32_t>
	, std::vector<VkClearValue>
	, ZDistType<SomeOne, std::any> // ZAttachmentPool
	, std::vector<ZDistType<SomeTwo, std::any>> // ZSubpassDescription2's
>
ZRenderPass;

void vtfDestroyFramebuffer(void_cptr, VkDevice, VkFramebuffer, const VkAllocationCallbacks*);
typedef ZDeletable<VkFramebuffer,
	decltype(&vtfDestroyFramebuffer), &vtfDestroyFramebuffer,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZDistType<Width, uint32_t>, ZDistType<Height, uint32_t>,
	ZRenderPass, std::vector<ZImageView>, uint32_t /*viewCount*/>
ZFramebuffer;

void vtfDestroySampler(void_cptr, VkDevice, VkSampler, const VkAllocationCallbacks*);
typedef ZDeletable<VkSampler,
	decltype(&vtfDestroySampler), &vtfDestroySampler,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkSamplerCreateInfo>
ZSampler;

struct type_index_with_default : public std::type_index
{
    std::size_t size = 0u;
	type_index_with_default () : std::type_index(typeid(void)) {}
	type_index_with_default (add_cref<std::type_info> info) : std::type_index(info) {}
	type_index_with_default (add_cref<std::type_index> index) : std::type_index(index) {}
	template<class X> static type_index_with_default make () {
        type_index_with_default tiwd(typeid(X));
        tiwd.size = sizeof(X);
        return tiwd;
	}
};

void vtfDestroyBuffer(void_cptr, VkDevice, VkBuffer, const VkAllocationCallbacks*);
typedef ZDeletable<VkBuffer,
	decltype(&vtfDestroyBuffer), &vtfDestroyBuffer,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkBufferCreateInfo, std::vector<ZDeviceMemory>, VkDeviceSize,
	type_index_with_default, VkExtent3D, VkFormat>
ZBuffer;

void vtfDestroyDescriptorPool(void_cptr, VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
typedef ZDeletable<VkDescriptorPool,
	decltype(&vtfDestroyDescriptorPool), &vtfDestroyDescriptorPool,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr>
ZDescriptorPool;

void freeDescriptorSet(void_cptr, VkDevice, VkDescriptorSet, VkDescriptorPool);
typedef ZDeletable<VkDescriptorSet,
	decltype(&freeDescriptorSet), &freeDescriptorSet,
	swizzle_four_params, ZDeletableBase, ZDevice, ZDescriptorPool>
ZDescriptorSet;

void vtfDestroyDescriptorSetLayout(void_cptr obj, VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
typedef ZDeletable<VkDescriptorSetLayout,
	decltype(&vtfDestroyDescriptorSetLayout), &vtfDestroyDescriptorSetLayout,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkDescriptorSetLayoutCreateFlags, ZDescriptorSet,
	ZDistType<LayoutIdentifier, uint32_t>>
ZDescriptorSetLayout;

void vtfDestroyPipelineLayout(void_cptr, VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
typedef ZDeletable<VkPipelineLayout,
	decltype(&vtfDestroyPipelineLayout), &vtfDestroyPipelineLayout,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	VkPipelineLayoutCreateFlags,
	std::vector<ZDescriptorSetLayout>,
	std::vector<VkPushConstantRange>,
	std::vector<type_index_with_default>,
	bool /*enableDescriptorBuffer*/>
ZPipelineLayout;

void vtfDestroyPipeline(void_cptr, VkDevice, VkPipeline, const VkAllocationCallbacks*);
typedef ZDeletable<VkPipeline,
	decltype(&vtfDestroyPipeline), &vtfDestroyPipeline,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	ZPipelineLayout, ZRenderPass, VkPipelineBindPoint, VkPipelineCreateFlags,
	std::vector<ZShaderModule>, // ray-tracing shaders
	ZDistType<LayoutIdentifier, uint32_t>, // ray-tracing pipeline shader group order
	ZDistType<Count, uint32_t> // ray-tracing pipeline shader group count
>
ZPipeline;

// Implemented in vtfZDeletable.cpp
struct ZPipelineCacheBase : ZDeletableBase { void saveToFile(); };
void freePipelineCache(ZDevice, VkPipelineCache, add_cref<std::string>, bool);
typedef ZDeletable<VkPipelineCache,
	decltype(&freePipelineCache), &freePipelineCache,
	std::integer_sequence<int, DontDereferenceParamOffset, -1, 2, 3>,
	ZPipelineCacheBase,	ZDevice, VkAllocationCallbacksPtr,
	std::string/*cacheFileName*/, bool/*saveOnDestroy*/>
ZPipelineCache;

void vtfDestroyQueryPool(void_cptr, VkDevice, VkQueryPool, const VkAllocationCallbacks*);
typedef ZDeletable<VkQueryPool,
	decltype(&vtfDestroyQueryPool), &vtfDestroyQueryPool,
	swizzle_four_params, ZDeletableBase, ZDevice, VkAllocationCallbacksPtr,
	uint32_t>
ZQueryPool;

#endif // __VTF_ZDELETABLE_HPP_INCLUDED__
 
