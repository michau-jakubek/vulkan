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

#define ARRAY_LENGTH(a) std::extent<decltype(a)>::value

#define UNREF(x) static_cast<void>(x)

#define UNUSED __attribute__((unused))

#define ROUNDUP(x__, multipler__) ((((x__)+((multipler__)-1))/(multipler__))*(multipler__))

#define VKASSERT2(expr__) {				\
	auto res__ = (expr__);				\
	if (res__ != VK_SUCCESS) {			\
		assertion( false, __func__, __FILE__, __LINE__, vkResultToString(res__)); \
	} \
}

#define VKASSERT3(expr__, msg__) {		\
	auto res__ = (expr__);				\
	if (res__ != VK_SUCCESS) {			\
		assertion( false, __func__, __FILE__, __LINE__, (std::string(msg__) + vkResultToString(res__))); \
	} \
}

struct Version
{
	uint32_t nmajor;
	uint32_t nminor;
	uint32_t npatch;
	Version (uint32_t v)
		: nmajor	(VK_VERSION_MAJOR(v))
		, nminor	(VK_VERSION_MINOR(v))
		, npatch	(VK_VERSION_PATCH(v))
	{ }
};
std::ostream& operator<<(std::ostream& str, const Version& v);

template<class ResultFlags, class Bits> //, class... OtherBits>
struct Flags
{
	static_assert(!std::is_same<ResultFlags, Bits>::value, "");
	//static_assert(std::negate<bool>(std::is_same<ResultFlags, Bits>::value), "");
	template<class... OtherBits>
	Flags(const Bits& bit, const OtherBits&... others)
		: Flags(nullptr, std::forward<const Bits>(bit), std::forward<const OtherBits>(others)...) {}
	Flags(const Flags& other) : m_flags(other.m_flags) {}
	operator ResultFlags() const { return m_flags; }
	ResultFlags operator()() const { return m_flags; }
	static Flags empty() { return Flags(); }
	bool isEmpty() const { return m_flags == empty(); }
	Flags& operator+=(Bits bit) { m_flags |= bit; return *this; }
	Flags& operator|=(Bits bit) { m_flags |= bit; return *this; }
	Flags& operator-=(Bits bit) { m_flags &= (~bit); return *this; }
protected:
	ResultFlags m_flags;
	Flags() : m_flags(0) {}
	template<class... OtherBits>
	Flags(std::nullptr_t sink, const Bits& bit, const OtherBits&... others)
		: Flags(sink, std::forward<const OtherBits>(others)...) { m_flags |= bit; }
	Flags(std::nullptr_t, const Bits& bit) { m_flags = bit; }
};
typedef Flags<VkBufferUsageFlags, VkBufferUsageFlagBits>		ZBufferUsageFlags;
typedef Flags<VkImageUsageFlags, VkImageUsageFlagBits>			ZImageUsageFlags;
typedef Flags<VkMemoryPropertyFlags, VkMemoryPropertyFlagBits>	ZMemoryPropertyFlags;
//
// types
//
template<class T,bool=false> std::bad_typeid type_to_vk_format;

template<> constexpr VkFormat type_to_vk_format<int8_t>			= VK_FORMAT_R8_SINT;
template<> constexpr VkFormat type_to_vk_format<int8_t,true>	= VK_FORMAT_R8_SNORM;
template<> constexpr VkFormat type_to_vk_format<uint8_t>		= VK_FORMAT_R8_UINT;
template<> constexpr VkFormat type_to_vk_format<uint8_t,true>	= VK_FORMAT_R8_UNORM;

template<> constexpr VkFormat type_to_vk_format<BVec1>			= VK_FORMAT_R8_SINT;
template<> constexpr VkFormat type_to_vk_format<BVec1,true>		= VK_FORMAT_R8_SNORM;
template<> constexpr VkFormat type_to_vk_format<UBVec1>			= VK_FORMAT_R8_UINT;
template<> constexpr VkFormat type_to_vk_format<UBVec1,true>	= VK_FORMAT_R8_UNORM;

template<> constexpr VkFormat type_to_vk_format<float>		= VK_FORMAT_R32_SFLOAT;
template<> constexpr VkFormat type_to_vk_format<int32_t>	= VK_FORMAT_R32_SINT;
template<> constexpr VkFormat type_to_vk_format<uint32_t>	= VK_FORMAT_R32_UINT;

template<> constexpr VkFormat type_to_vk_format<WVec1>		=          VK_FORMAT_R16_SNORM;
template<> constexpr VkFormat type_to_vk_format<WVec2>		=       VK_FORMAT_R16G16_SNORM;
template<> constexpr VkFormat type_to_vk_format<WVec3>		=    VK_FORMAT_R16G16B16_SNORM;
template<> constexpr VkFormat type_to_vk_format<WVec4>		= VK_FORMAT_R16G16B16A16_SNORM;

template<> constexpr VkFormat type_to_vk_format<Vec1>		=          VK_FORMAT_R32_SFLOAT;
template<> constexpr VkFormat type_to_vk_format<Vec2>		=       VK_FORMAT_R32G32_SFLOAT;
template<> constexpr VkFormat type_to_vk_format<Vec3>		=    VK_FORMAT_R32G32B32_SFLOAT;
template<> constexpr VkFormat type_to_vk_format<Vec4>		= VK_FORMAT_R32G32B32A32_SFLOAT;

template<> constexpr VkFormat type_to_vk_format<IVec1>		=          VK_FORMAT_R32_SINT;
template<> constexpr VkFormat type_to_vk_format<IVec2>		=       VK_FORMAT_R32G32_SINT;
template<> constexpr VkFormat type_to_vk_format<IVec3>		=    VK_FORMAT_R32G32B32_SINT;
template<> constexpr VkFormat type_to_vk_format<IVec4>		= VK_FORMAT_R32G32B32A32_SINT;

template<> constexpr VkFormat type_to_vk_format<UVec1>		=          VK_FORMAT_R32_UINT;
template<> constexpr VkFormat type_to_vk_format<UVec2>		=       VK_FORMAT_R32G32_UINT;
template<> constexpr VkFormat type_to_vk_format<UVec3>		=    VK_FORMAT_R32G32B32_UINT;
template<> constexpr VkFormat type_to_vk_format<UVec4>		= VK_FORMAT_R32G32B32A32_UINT;

template<VkFormat,bool> struct vk_format_to_type_impl;
template<VkFormat fmt, bool agg = true>
	using vk_format_to_type = typename vk_format_to_type_impl<fmt,agg>::type;
#define VKFORMATTOTYPEIMPL(aggregated_, format_, type_)				\
	template<> struct vk_format_to_type_impl<format_,aggregated_> { typedef type_ type; }

VKFORMATTOTYPEIMPL(true,			VK_FORMAT_R8_SINT,	BVec1);
VKFORMATTOTYPEIMPL(true,			VK_FORMAT_R8_UINT,	UBVec1);
VKFORMATTOTYPEIMPL(true,			VK_FORMAT_R8_UNORM,	UBVec1);

VKFORMATTOTYPEIMPL(true,          VK_FORMAT_R32_SFLOAT, Vec1);
VKFORMATTOTYPEIMPL(true,       VK_FORMAT_R32G32_SFLOAT, Vec2);
VKFORMATTOTYPEIMPL(true,    VK_FORMAT_R32G32B32_SFLOAT, Vec3);
VKFORMATTOTYPEIMPL(true, VK_FORMAT_R32G32B32A32_SFLOAT, Vec4);

VKFORMATTOTYPEIMPL(true,          VK_FORMAT_R32_SINT, IVec1);
VKFORMATTOTYPEIMPL(true,       VK_FORMAT_R32G32_SINT, IVec2);
VKFORMATTOTYPEIMPL(true,    VK_FORMAT_R32G32B32_SINT, IVec3);
VKFORMATTOTYPEIMPL(true, VK_FORMAT_R32G32B32A32_SINT, IVec4);

VKFORMATTOTYPEIMPL(true,          VK_FORMAT_R32_UINT, UVec1);
VKFORMATTOTYPEIMPL(true,       VK_FORMAT_R32G32_UINT, UVec2);
VKFORMATTOTYPEIMPL(true,    VK_FORMAT_R32G32B32_UINT, UVec3);
VKFORMATTOTYPEIMPL(true, VK_FORMAT_R32G32B32A32_UINT, UVec4);

VKFORMATTOTYPEIMPL(true,          VK_FORMAT_R16_SNORM, WVec1);
VKFORMATTOTYPEIMPL(true,       VK_FORMAT_R16G16_SNORM, WVec2);
VKFORMATTOTYPEIMPL(true,    VK_FORMAT_R16G16B16_SNORM, WVec3);
VKFORMATTOTYPEIMPL(true, VK_FORMAT_R16G16B16A16_SNORM, WVec4);

VKFORMATTOTYPEIMPL(true,          VK_FORMAT_R8_SNORM, BVec1);
VKFORMATTOTYPEIMPL(true,        VK_FORMAT_R8G8_SNORM, BVec2);
VKFORMATTOTYPEIMPL(true,      VK_FORMAT_R8G8B8_SNORM, BVec3);
VKFORMATTOTYPEIMPL(true,    VK_FORMAT_R8G8B8A8_SNORM, BVec4);

//
// function templates
//

// Be careful, source strings must be alive after to_strings() is invoked.
template<template<class T, class... U> class C, class T, class... U>
void to_cstrings(const C<T, U...>& iss, std::vector<const char*>& oss)
{
	oss.resize(iss.size());
	std::transform(iss.begin(), iss.end(), oss.begin(), [](const std::string& s) { return s.data(); });
}
template<template<class T, class... U> class C, class T, class... U>
std::vector<const char*> to_cstrings(const C<T, U...>& iss)
{
	std::vector<const char*> oss;
	to_cstrings(iss, oss);
	return oss;
}

//
// regular routines
//
uint32_t			findQueueFamilyIndex (VkPhysicalDevice phDevice, VkQueueFlagBits bit);
uint32_t			findSurfaceSupportedQueueFamilyIndex (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
bool				hasFormatsAndModes (VkPhysicalDevice physDevice, VkSurfaceKHR surfaceKHR);
VkClearValue		makeClearColor (const Vec4& color);
const char*			vkResultToString (VkResult res);
strings				enumerateInstanceLayers ();
strings				enumerateInstanceExtensions (const strings& layerNames = {});
strings				enumerateDeviceExtensions (VkPhysicalDevice device, const strings& layerNames = {});
uint32_t			enumeratePhysicalDevices (VkInstance instance, std::vector<VkPhysicalDevice>& devices);
uint32_t			enumerateSwapchainImages (VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images);
std::ostream&		printPhysicalDevices (VkInstance instance, std::ostream& str);

} // namespace vtf

#endif // __VTF_VK_UTILS_HPP_INCLUDED__
