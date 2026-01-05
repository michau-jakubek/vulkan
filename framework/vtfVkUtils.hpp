#ifndef __VTF_VK_UTILS_HPP_INCLUDED__
#define __VTF_VK_UTILS_HPP_INCLUDED__

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <limits>

#include "vulkan/vulkan.h"
#include "vtfVector.hpp"
#include "vtfCUtils.hpp"

namespace vtf
{

constexpr uint16_t INVALID_UINT16 = std::numeric_limits<uint16_t>::max();
constexpr uint32_t INVALID_UINT32 = std::numeric_limits<uint32_t>::max();
constexpr uint64_t INVALID_UINT64 = std::numeric_limits<uint64_t>::max();

#define MULTIPLERUP(x__, multipler__) (((x__)+((multipler__)-1))/(multipler__))
#define ROUNDUP(x__, multipler__) (MULTIPLERUP((x__),(multipler__))*(multipler__))
#define ROUNDDOWN(x__, multipler__) (((x__)/(multipler__))*(multipler__))

#define VKASSERT(expr__) { \
	const VkResult rez__ = (expr__); if (rez__ != VK_SUCCESS) \
	ASSERT_IMPL( (rez__ == VK_SUCCESS), STRINGIZE(expr__), rez__, false, 0); }

#define VKASSERTMSG(expr__, ...) { \
	const VkResult rez__ = (expr__); if (rez__ != VK_SUCCESS) \
	ASSERT_IMPL( (rez__ == VK_SUCCESS), STRINGIZE(expr__), rez__, true, __VA_ARGS__); }

struct Version
{
	uint32_t nmajor;
	uint32_t nminor;
	uint32_t npatch;
	uint32_t nvariant;
	constexpr	Version (uint32_t major, uint32_t minor)
					: nmajor	(major)
					, nminor	(minor)
					, npatch	(0)
					, nvariant	(0)
				{ }
	constexpr	Version	(uint32_t major, uint32_t minor, uint32_t patch, uint32_t variant)
					: nmajor	(major)
					, nminor	(minor)
					, npatch	(patch)
					, nvariant	(variant)
				{ }
	void update (uint32_t v)
	{
		nmajor		= (VK_API_VERSION_MAJOR(v));
		nminor		= (VK_API_VERSION_MINOR(v));
		npatch		= (VK_API_VERSION_PATCH(v));
		nvariant	= (VK_API_VERSION_VARIANT(v))		;
	}
	static Version fromUint (uint32_t v)
	{
		Version ver(0, 0);
		ver.update(v);
		return ver;
	}
	static Version from10xMajorPlusMinor (uint32_t v)
	{
		Version		x(0, 0);
		x.npatch	= 0;
		x.nvariant	= 0;
		x.nminor	= v % 10;
		x.nmajor	= v / 10;
		return x;
	}
	uint32_t to10xMajorPlusMinor () const
	{
		return nmajor * 10 + nminor;
	}
	bool operator< (const Version& other) const
	{
		return (nmajor < other.nmajor) || (nminor < other.nminor);
	}
	operator uint32_t () const
	{
		return VK_MAKE_API_VERSION(nvariant, nmajor, nminor, npatch);
	}
	bool isValid () const
	{
		return nmajor != 0u;
	}
	explicit operator bool () const
	{
		return isValid();
	}
	static uint32_t make (uint32_t major, uint32_t minor, uint32_t variant = 0, uint32_t patch = 0)
	{
		return VK_MAKE_API_VERSION(variant, major, minor, patch);
	}
	std::string toString() const
	{
		return std::to_string(nmajor) + '.' + std::to_string(nminor);
	}
};
typedef ZDistType<VtfVer, Version> VtfVersion;
typedef ZDistType<ApiVer, Version> ApiVersion;
typedef ZDistType<VulkanVer, Version> VulkanVersion;
typedef ZDistType<SpirvVer, Version> SpirvVersion;
std::ostream& operator<<(std::ostream& str, const Version& v);
std::ostream& operator<<(std::ostream& str, const VtfVersion& v);

template<class ResultFlags, class Bits> //, class... OtherBits>
struct Flags
{
	static_assert(!std::is_same<ResultFlags, Bits>::value, "");
	constexpr Flags() : m_flags{} {}
	template<class... OtherBits>
	constexpr Flags(const Bits& bit, const OtherBits&... others) : Flags(nullptr, bit, others...) {}
	constexpr explicit operator ResultFlags() const { return m_flags; }
	constexpr ResultFlags operator()() const { return m_flags; }
	static constexpr Flags empty() { return Flags(ResultFlags(0)); }
	constexpr bool isEmpty() const { return m_flags == ResultFlags(0); }
	constexpr Flags& operator+=(Bits bit) { m_flags |= bit; return *this; }
	constexpr Flags& operator|=(Bits bit) { m_flags |= bit; return *this; }
	constexpr Flags& operator-=(Bits bit) { m_flags &= (~bit); return *this; }
	constexpr Flags operator+(Bits bit) const { Flags f(*this); f.m_flags |= bit; return f; }
	constexpr Flags operator-(Bits bit) const { Flags f(*this); f.m_flags &= (~bit); return f; }
	static constexpr Flags fromFlags(const ResultFlags& flags) { return Flags(flags); }
	constexpr bool contain(const Bits& bit) const { return ((m_flags & ResultFlags(bit)) == ResultFlags(bit)); }
protected:
	ResultFlags m_flags;
	constexpr Flags(const ResultFlags& flags) : m_flags(flags) {}
	template<class... OtherBits>
	constexpr Flags(std::nullptr_t sink, const Bits& bit, const OtherBits&... others)
		: Flags(sink, others...) { m_flags |= bit; }
	constexpr Flags(std::nullptr_t, const Bits& bit) : m_flags(ResultFlags(bit)) {}
};

typedef Flags<VkBufferUsageFlags, VkBufferUsageFlagBits>			ZBufferUsageFlags;
typedef Flags<VkBufferCreateFlags, VkBufferCreateFlagBits>			ZBufferCreateFlags;
typedef Flags<VkImageUsageFlags, VkImageUsageFlagBits>				ZImageUsageFlags;
typedef Flags<VkMemoryPropertyFlags, VkMemoryPropertyFlagBits>		ZMemoryPropertyFlags;
typedef Flags<VkPipelineCreateFlags, VkPipelineCreateFlagBits>		ZPipelineCreateFlags;
typedef Flags<VkRenderingFlags, VkRenderingFlagBits>				ZRenderingFlags;
static const ZBufferUsageFlags ZBufferUsageStorageFlags(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
static const ZBufferUsageFlags ZBufferUsageUniformFlags(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
static const ZMemoryPropertyFlags ZMemoryPropertyDeviceFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
static const ZMemoryPropertyFlags ZMemoryPropertyHostFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

uint32_t		findQueueFamilyIndex (VkPhysicalDevice phDevice, VkQueueFlagBits bit);
uint32_t		findSurfaceSupportedQueueFamilyIndex (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
std::vector<uint32_t> findSurfaceSupportedQueueFamilyIndices (VkPhysicalDevice physDevice, ZSurfaceKHR surface);
bool			hasFormatsAndModes (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
std::string		vkResultToString (VkResult res);
strings			enumerateInstanceLayers ();
strings			enumerateInstanceExtensions (const char* layerName);
strings			enumerateInstanceExtensions (const strings& layerNames = {});
strings			enumerateDeviceExtensions (VkPhysicalDevice device, const strings& layerNames = {});
uint32_t		enumeratePhysicalDevices (VkInstance instance, std::vector<VkPhysicalDevice>& devices);
uint32_t		enumerateSwapchainImages (VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images);
std::ostream&	printPhysicalDevices (VkInstance instance, std::ostream& str);
std::ostream&	printPhysicalDevice (VkPhysicalDevice device, std::ostream& str, uint32_t deviceIndex = INVALID_UINT32);
std::ostream&   printPhysicalDevice (add_cref<VkPhysicalDeviceProperties> props, std::ostream& str, uint32_t deviceIndex = INVALID_UINT32);
add_ref<std::ostream> printPhysicalDeviceFeatures (add_cref<VkPhysicalDeviceFeatures>, add_ref<std::ostream> str, uint32_t indent);

uint32_t		computePixelByteSize (VkFormat format);
uint32_t		computePixelChannelCount (VkFormat format);

/**
 * @brief Calculates available mip level count starting from given width and height
 */
uint32_t		computeMipLevelCount (uint32_t width, uint32_t height);

/**
 * @brief Calculates a dimension of hypothetical mip level assuming that width and height belong to level 0
 */
auto			computeMipLevelWidthAndHeight (uint32_t width, uint32_t height, uint32_t level) -> std::pair<uint32_t,uint32_t>;

/**
 * @brief Counts the sum of the texels on each mip level where width and height are the dimension of level 0
 */
VkDeviceSize	computeMipLevelsPixelCount (uint32_t width, uint32_t height, uint32_t levelCount);

/**
 * @brief Calculates offset and size of hypothetical mip level
 */
auto			computeMipLevelsOffsetAndSize (VkFormat format, uint32_t level0Width, uint32_t level0Height,
											   uint32_t baseMipLevel, uint32_t mipLevelCount) -> std::pair<VkDeviceSize, VkDeviceSize>;


uint32_t		sampleFlagsToSampleCount (VkSampleCountFlags flags);
VkExtent2D		makeExtent2D (uint32_t width, uint32_t height);
VkExtent2D		makeExtent2D (add_cref<VkExtent3D> extent3D);
VkRect2D		makeRect2D (uint32_t width, uint32_t height, int32_t Xoffset = 0u, int32_t Yoffset = 0u);
VkRect2D		makeRect2D (add_cref<VkExtent2D> extent2D, add_cref<VkOffset2D> offset2D = {});
VkExtent3D		makeExtent3D (uint32_t width = 0u, uint32_t height = 0u, uint32_t depth = 0u);
VkOffset2D		makeOffset2D (int32_t x = 0, int32_t y = 0);
VkOffset3D		makeOffset3D (int32_t x = 0, int32_t y = 0, int32_t z = 0);
VkViewport		makeViewport (uint32_t width, uint32_t height, uint32_t x = 0, uint32_t y = 0, float minDepth = 0.0f, float maxDepth = +1.0);
VkViewport		makeViewport (add_cref<VkExtent2D> extent, float minDepth = 0.0f, float maxDepth = +1.0);
VkRect2D		clampScissorToViewport (add_cref<VkViewport> viewport, add_ref<VkRect2D> inOutScissor);
template<class CompType> // int32_t: IVec4, uint32_t: UVec4, float: Vec4
VkClearColorValue makeClearColorValue (const VecX<CompType,4>& color);
template<class CompType> // int32_t: IVec4, uint32_t: UVec4, float: Vec4
VkClearValue      makeClearColor(const VecX<CompType, 4>& color) { return VkClearValue{ makeClearColorValue(color) }; }
VkColorBlendEquationEXT makeColorBlendEquationExt (add_cref<VkPipelineColorBlendAttachmentState> state);

std::ostream&	operator<<(std::ostream& str, add_cref<ZDistType<QueueFlags, VkQueueFlags>> flags);
std::ostream&	operator<<(std::ostream& str, add_cref<ZDeviceQueueCreateInfo> props);
std::ostream&	operator<<(std::ostream& str, add_cref<VkPrimitiveTopology> topo);
std::ostream&	operator<<(std::ostream& str, add_cref<VkShaderStageFlagBits> stage);

struct PNextChain
{
	PNextChain() { release(); }
	PNextChain(add_ptr<void_ptr> ppNext_) { reset(ppNext_); }
	void_ptr release()
	{
		void_ptr ptr = pNext;
		pNext = nullptr;
		ppNext = &pNext;
		return ptr;
	}
	void_ptr reset(add_ptr<void_ptr> ppNext_)
	{
		ASSERTMSG(nullptr != ppNext_, "ppNext must not be null");
		void_ptr ptr = pNext;
		ppNext = ppNext_;
		pNext = *ppNext;
		return ptr;
	}
	void append(void_ptr next)
	{
		ASSERTMSG(nullptr != ppNext, "ppNext must not be null, call reset(...)");
		ASSERTMSG(nullptr != next, "next must not be null");
		*ppNext = next;
		ppNext = reinterpret_cast<add_ptr<void_ptr>>(&static_cast<add_ptr<VkBaseOutStructure>>(next)->pNext);
	}
private:
	void_ptr pNext;
	add_ptr<void_ptr> ppNext;
};
} // namespace vtf

#endif // __VTF_VK_UTILS_HPP_INCLUDED__
