#ifndef __VTF_FORMAT_UTILS_HPP_INCLUDED__
#define __VTF_FORMAT_UTILS_HPP_INCLUDED__

#include "vulkan/vulkan.h"
#include "vtfZDeletable.hpp"
#include "vtfVector.hpp"
#include "vtfFloat16.hpp"

#include <iostream>
#include <cstring>
#include <variant>


namespace vtf
{

/**
 * Disadvantages: all instance contsist of the same owner class reference
 * template<class C, class X> struct add_get
 * {
 * 	typedef X (C::* getter_t)(X C::*);
 * 	add_ref<C>	behalf;
 * 	getter_t	getter;
 * 	add_get(add_ref<C> c, getter_t ptr)
 * 		: behalf(c), getter(ptr) {}
 * 	operator X() { return (*behalf.ptr)(getter); }
 * };
 */

/**
template<class C, class FieldType> struct add_get;

template<class Behalf, class FieldType>
struct GetFieldInfo
{
	//typedef FieldType (Behalf::* GetMethodPtr)(FieldType Behalf::*);
	typedef FieldType (Behalf::* GetMethodPtr)(add_get<Behalf, FieldType> Behalf::*);
	add_ref<Behalf>	behalf;
	GetMethodPtr	method;
	GetFieldInfo (add_ref<Behalf> call_on, GetMethodPtr getMehodPtr)
		: behalf(call_on), method(getMehodPtr) {}
};

template<class C, class FieldType> struct add_get
{
	GetFieldInfo<C, FieldType> getFieldInfo;
	add_get(add_ref<GetFieldInfo<C, FieldType>> getter) : getFieldInfo(getter) {}
	operator FieldType() { return {}; } //return (*((getter.behalf).getter.fieldPtr))(getter.ptr); }
};
*/

struct ZFormatInfo;

struct ZFormatInfo
{
	ZFormatInfo ();
	const char*		name;
	VkFormat		format;
	int8_t			swizzling[4];		// [3210] -> ABGR
	uint8_t			componentBitSizes[4];	// in RGBA order
	uint8_t			componentByteSizes[4];	// in RGBA order
	uint32_t		componentCount;
	uint32_t		pixelByteSize;
	uint32_t		pack;
	bool			isSigned;
	bool			normalized;
	bool			floating;
	bool			integral;
	bool			scaled;
	bool			sized;
	bool			color;
	bool			stencil;
};

class ZFormatInfoIterator : public ZFormatInfo
{
	int m_current;
public:

	ZFormatInfoIterator ();
	void reset ();
	bool next ();
};

const char*			formatGetString		(VkFormat format);
ZFormatInfo			formatGetInfo		(VkFormat format);
VkImageAspectFlags	formatGetAspectMask	(VkFormat format);
VkComponentMapping	makeComponentMapping (const ZFormatInfo& info);
VkFormatProperties	formatGetProperties	(ZPhysicalDevice device, VkFormat format);
bool				formatSupportsLinearFlags (ZPhysicalDevice device, VkFormat format, VkFormatFeatureFlags);
bool				formatSupportsLinearFilter (ZPhysicalDevice device, VkFormat format);
/* <is-depth-or-stencli, is-stencil-also> */
std::pair<bool, bool> formatIsDepthStencil (ZPhysicalDevice device, VkFormat format, bool checkOptimalTilingSupport = false);

enum FmtMask
{
	General,
	Normalized,
	Scaled
};

template<class VecY_, uint32_t Mask_ = FmtMask::General>
struct Fmt_ : public VecX<vecx_type<VecY_>, vecx_count<VecY_>>
{
	typedef VecY_ type;
};

template<class> std::bad_typeid type_to_vk_format;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec1>>			= VK_FORMAT_R8_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec2>>			= VK_FORMAT_R8G8_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec3>>			= VK_FORMAT_R8G8B8_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec4>>			= VK_FORMAT_R8G8B8A8_SINT;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec1>>			= VK_FORMAT_R8_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec2>>			= VK_FORMAT_R8G8_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec3>>			= VK_FORMAT_R8G8B8_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec4>>			= VK_FORMAT_R8G8B8A8_UINT;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec1,FmtMask::Normalized>>		= VK_FORMAT_R8_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec2,FmtMask::Normalized>>		= VK_FORMAT_R8G8_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec3,FmtMask::Normalized>>		= VK_FORMAT_R8G8B8_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<BVec4,FmtMask::Normalized>>		= VK_FORMAT_R8G8B8A8_SNORM;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec1,FmtMask::Normalized>>		= VK_FORMAT_R8_UNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec2,FmtMask::Normalized>>		= VK_FORMAT_R8G8_UNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec3,FmtMask::Normalized>>		= VK_FORMAT_R8G8B8_UNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UBVec4,FmtMask::Normalized>>		= VK_FORMAT_R8G8B8A8_UNORM;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<WVec1,FmtMask::Normalized>>		=          VK_FORMAT_R16_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<WVec2,FmtMask::Normalized>>		=       VK_FORMAT_R16G16_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<WVec3,FmtMask::Normalized>>		=    VK_FORMAT_R16G16B16_SNORM;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<WVec4,FmtMask::Normalized>>		= VK_FORMAT_R16G16B16A16_SNORM;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<float>>		=          VK_FORMAT_R32_SFLOAT;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<Vec1>>		=          VK_FORMAT_R32_SFLOAT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<Vec2>>		=       VK_FORMAT_R32G32_SFLOAT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<Vec3>>		=    VK_FORMAT_R32G32B32_SFLOAT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<Vec4>>		= VK_FORMAT_R32G32B32A32_SFLOAT;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<IVec1>>		=          VK_FORMAT_R32_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<IVec2>>		=       VK_FORMAT_R32G32_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<IVec3>>		=    VK_FORMAT_R32G32B32_SINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<IVec4>>		= VK_FORMAT_R32G32B32A32_SINT;

template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UVec1>>		=          VK_FORMAT_R32_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UVec2>>		=       VK_FORMAT_R32G32_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UVec3>>		=    VK_FORMAT_R32G32B32_UINT;
template<> inline constexpr VkFormat type_to_vk_format<Fmt_<UVec4>>		= VK_FORMAT_R32G32B32A32_UINT;

template<VkFormat> struct vk_format_to_type_impl;
template<VkFormat fmt>
	using vk_format_to_type = typename vk_format_to_type_impl<fmt>::type::type;
// Ugly due to improper treating of commas when it occures inside template instantation
// VKFORMATTOTYPEIMPL(         VK_FORMAT_R8_SNORM, Fmt_<BVec1,Normalized>);
// Compiler sees three parameters instead of two
#define VKFORMATTOTYPEIMPL(format_, type_, mask_)				\
	template<> struct vk_format_to_type_impl<format_> { typedef Fmt_<type_,mask_> type; }

VKFORMATTOTYPEIMPL(			VK_FORMAT_R8_SINT, BVec1, General);
VKFORMATTOTYPEIMPL(		  VK_FORMAT_R8G8_SINT, BVec2, General);
VKFORMATTOTYPEIMPL(		VK_FORMAT_R8G8B8_SINT, BVec3, General);
VKFORMATTOTYPEIMPL(	  VK_FORMAT_R8G8B8A8_SINT, BVec4, General);

VKFORMATTOTYPEIMPL(			VK_FORMAT_R8_UINT, UBVec1, General);
VKFORMATTOTYPEIMPL(		  VK_FORMAT_R8G8_UINT, UBVec2, General);
VKFORMATTOTYPEIMPL(		VK_FORMAT_R8G8B8_UINT, UBVec3, General);
VKFORMATTOTYPEIMPL(	  VK_FORMAT_R8G8B8A8_UINT, UBVec4, General);

VKFORMATTOTYPEIMPL(         VK_FORMAT_R8_SNORM, BVec1, Normalized);
VKFORMATTOTYPEIMPL(       VK_FORMAT_R8G8_SNORM, BVec2, Normalized);
VKFORMATTOTYPEIMPL(     VK_FORMAT_R8G8B8_SNORM, BVec3, Normalized);
VKFORMATTOTYPEIMPL(   VK_FORMAT_R8G8B8A8_SNORM, BVec4, Normalized);

VKFORMATTOTYPEIMPL(         VK_FORMAT_R8_UNORM, UBVec1, Normalized);
VKFORMATTOTYPEIMPL(       VK_FORMAT_R8G8_UNORM, UBVec2,	Normalized);
VKFORMATTOTYPEIMPL(     VK_FORMAT_R8G8B8_UNORM, UBVec3, Normalized);
VKFORMATTOTYPEIMPL(   VK_FORMAT_R8G8B8A8_UNORM, UBVec4, Normalized);

//VKFORMATTOTYPEIMPL(          VK_FORMAT_R16_SNORM, Fmt_<WVec1>);
//VKFORMATTOTYPEIMPL(       VK_FORMAT_R16G16_SNORM, Fmt_<WVec2>);
//VKFORMATTOTYPEIMPL(    VK_FORMAT_R16G16B16_SNORM, Fmt_<WVec3>);
//VKFORMATTOTYPEIMPL( VK_FORMAT_R16G16B16A16_SNORM, Fmt_<WVec4>);

VKFORMATTOTYPEIMPL(          VK_FORMAT_R32_SINT, IVec1, General);
VKFORMATTOTYPEIMPL(       VK_FORMAT_R32G32_SINT, IVec2, General);
VKFORMATTOTYPEIMPL(    VK_FORMAT_R32G32B32_SINT, IVec3, General);
VKFORMATTOTYPEIMPL( VK_FORMAT_R32G32B32A32_SINT, IVec4, General);

VKFORMATTOTYPEIMPL(          VK_FORMAT_R32_UINT, UVec1, General);
VKFORMATTOTYPEIMPL(       VK_FORMAT_R32G32_UINT, UVec2, General);
VKFORMATTOTYPEIMPL(    VK_FORMAT_R32G32B32_UINT, UVec3, General);
VKFORMATTOTYPEIMPL( VK_FORMAT_R32G32B32A32_UINT, UVec4, General);

VKFORMATTOTYPEIMPL(          VK_FORMAT_R32_SFLOAT, Vec1, General);
VKFORMATTOTYPEIMPL(       VK_FORMAT_R32G32_SFLOAT, Vec2, General);
VKFORMATTOTYPEIMPL(    VK_FORMAT_R32G32B32_SFLOAT, Vec3, General);
VKFORMATTOTYPEIMPL( VK_FORMAT_R32G32B32A32_SFLOAT, Vec4, General);

uint32_t getComponentByteSize (VkComponentTypeKHR c);

template<class> VkComponentTypeKHR ctype_to_vk_component_type = VK_COMPONENT_TYPE_MAX_ENUM_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<Float16>	= VK_COMPONENT_TYPE_FLOAT16_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<float>	= VK_COMPONENT_TYPE_FLOAT32_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<double>	= VK_COMPONENT_TYPE_FLOAT64_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<int8_t>	= VK_COMPONENT_TYPE_SINT8_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<int16_t>	= VK_COMPONENT_TYPE_SINT16_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<int32_t>	= VK_COMPONENT_TYPE_SINT32_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<int64_t>	= VK_COMPONENT_TYPE_SINT64_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<uint8_t>	= VK_COMPONENT_TYPE_UINT8_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<uint16_t>	= VK_COMPONENT_TYPE_UINT16_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<uint32_t>	= VK_COMPONENT_TYPE_UINT32_KHR;
template<> inline constexpr VkComponentTypeKHR ctype_to_vk_component_type<uint64_t>	= VK_COMPONENT_TYPE_UINT64_KHR;

} // vtf

#endif // __VTF_FORMAT_UTILS_HPP_INCLUDED__
