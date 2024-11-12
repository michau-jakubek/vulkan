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

#include "vulkan/vulkan.h"
#include "vtfVector.hpp"
#include "vtfCUtils.hpp"

namespace vtf
{

constexpr uint32_t INVALID_UINT32 = (~(static_cast<uint32_t>(0u)));
constexpr uint64_t INVALID_UINT64 = (~(static_cast<uint64_t>(0u)));

#define MULTIPLERUP(x__, multipler__) (((x__)+((multipler__)-1))/(multipler__))
#define ROUNDUP(x__, multipler__) (MULTIPLERUP((x__),(multipler__))*(multipler__))
#define ROUNDDOWN(x__, multipler__) (((x__)/(multipler__))*(multipler__))

#define VKASSERT2(expr__) {				\
	auto res__ = (expr__);				\
	if (res__ != VK_SUCCESS) {			\
		assertion( false, __func__, __FILE__, __LINE__, vkResultToString(res__)); \
	} \
}

#define VKASSERT3(expr__, msg__) {		\
	auto res__ = (expr__);				\
	if (res__ != VK_SUCCESS) {			\
		assertion( false, __func__, __FILE__, __LINE__, (std::string(msg__) + ' ' + vkResultToString(res__))); \
	} \
}

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
	constexpr	Version	(uint32_t major, uint32_t minor, uint32_t path, uint32_t variant)
					: nmajor	(major)
					, nminor	(minor)
					, npatch	(path)
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
	Flags() : m_flags{} {}
	template<class... OtherBits>
	Flags(const Bits& bit, const OtherBits&... others)
		: Flags(nullptr, std::forward<const Bits>(bit), std::forward<const OtherBits>(others)...) {}
	// Implicit definition is deprecated because it provided by the compiler
	// Flags(const Flags& other) : m_flags(other.m_flags) {}
	explicit operator ResultFlags() const { return m_flags; }
	ResultFlags operator()() const { return m_flags; }
	static Flags empty() { return Flags(ResultFlags(0)); }
	bool isEmpty() const { return m_flags == empty(); }
	Flags& operator+=(Bits bit) { m_flags |= bit; return *this; }
	Flags& operator|=(Bits bit) { m_flags |= bit; return *this; }
	Flags& operator-=(Bits bit) { m_flags &= (~bit); return *this; }
	Flags operator+(Bits bit) const { Flags f(*this); f.m_flags |= bit; return f; }
	Flags operator-(Bits bit) const { Flags f(*this); f.m_flags &= (~bit); return f; }
	static Flags fromFlags(const ResultFlags& flags) { return Flags(flags); }
	bool contain(const Bits& bit) const { return ((m_flags & ResultFlags(bit)) == ResultFlags(bit)); }
protected:
	ResultFlags m_flags;
	Flags(const ResultFlags& flags) : m_flags(flags) {}
	template<class... OtherBits>
	Flags(std::nullptr_t sink, const Bits& bit, const OtherBits&... others)
		: Flags(sink, std::forward<const OtherBits>(others)...) { m_flags |= bit; }
	Flags(std::nullptr_t, const Bits& bit) { m_flags = ResultFlags(bit); }
};
typedef Flags<VkBufferUsageFlags, VkBufferUsageFlagBits>			ZBufferUsageFlags;
typedef Flags<VkBufferCreateFlags, VkBufferCreateFlagBits>			ZBufferCreateFlags;
typedef Flags<VkImageUsageFlags, VkImageUsageFlagBits>				ZImageUsageFlags;
typedef Flags<VkMemoryPropertyFlags, VkMemoryPropertyFlagBits>		ZMemoryPropertyFlags;
typedef Flags<VkPipelineCreateFlags, VkPipelineCreateFlagBits>		ZPipelineCreateFlags;
typedef Flags<VkRenderingFlags, VkRenderingFlagBits>				ZRenderingFlags;
static const ZMemoryPropertyFlags ZMemoryPropertyDeviceFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
static const ZMemoryPropertyFlags ZMemoryPropertyHostFlags(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

uint32_t		findQueueFamilyIndex (VkPhysicalDevice phDevice, VkQueueFlagBits bit);
uint32_t		findSurfaceSupportedQueueFamilyIndex (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
std::vector<uint32_t> findSurfaceSupportedQueueFamilyIndices (VkPhysicalDevice physDevice, ZSurfaceKHR surface);
bool			hasFormatsAndModes (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
const char*		vkResultToString (VkResult res);
strings			enumerateInstanceLayers ();
strings			enumerateInstanceExtensions (const strings& layerNames = {});
strings			enumerateDeviceExtensions (VkPhysicalDevice device, const strings& layerNames = {});
uint32_t		enumeratePhysicalDevices (VkInstance instance, std::vector<VkPhysicalDevice>& devices);
uint32_t		enumerateSwapchainImages (VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images);
std::ostream&	printPhysicalDevices (VkInstance instance, std::ostream& str);
std::ostream&	printPhysicalDevice (VkPhysicalDevice device, std::ostream& str, uint32_t deviceIndex = INVALID_UINT32);
std::ostream&   printPhysicalDevice (add_cref<VkPhysicalDeviceProperties> props, std::ostream& str, uint32_t deviceIndex = INVALID_UINT32);

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
VkRect2D		clampScissorToViewport (add_cref<VkViewport> viewport, add_ref<VkRect2D> inOutScissor);
template<class CompType> // int32_t: IVec4, uint32_t: UVec4, float: Vec4
VkClearColorValue makeClearColorValue (const VecX<CompType,4>& color);

std::ostream&	operator<<(std::ostream& str, add_cref<ZDistType<QueueFlags, VkQueueFlags>> flags);
std::ostream&	operator<<(std::ostream& str, add_cref<ZDeviceQueueCreateInfo> props);
std::ostream&	operator<<(std::ostream& str, add_cref<VkPrimitiveTopology> topo);
std::ostream&	operator<<(std::ostream& str, add_cref<VkShaderStageFlagBits> stage);

} // namespace vtf

#endif // __VTF_VK_UTILS_HPP_INCLUDED__
