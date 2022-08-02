#include "vtfFormatUtils.hpp"
#include "vtfVkUtils.hpp"
#include "vtfZDeletable.hpp"

namespace
{
using namespace vtf;
#define MKFN(format__) { format__, #format__ }

struct FormatAndName
{
	VkFormat          format;
	const char* const name;
}
const formatAndNames[]
{
	MKFN(VK_FORMAT_R4G4_UNORM_PACK8),
	MKFN(VK_FORMAT_R4G4B4A4_UNORM_PACK16),
	MKFN(VK_FORMAT_B4G4R4A4_UNORM_PACK16),
	MKFN(VK_FORMAT_R5G6B5_UNORM_PACK16),
	MKFN(VK_FORMAT_B5G6R5_UNORM_PACK16),
	MKFN(VK_FORMAT_R5G5B5A1_UNORM_PACK16),
	MKFN(VK_FORMAT_B5G5R5A1_UNORM_PACK16),
	MKFN(VK_FORMAT_A1R5G5B5_UNORM_PACK16),
	MKFN(VK_FORMAT_R8_UNORM),
	MKFN(VK_FORMAT_R8_SNORM),
	MKFN(VK_FORMAT_R8_USCALED),
	MKFN(VK_FORMAT_R8_SSCALED),
	MKFN(VK_FORMAT_R8_UINT),
	MKFN(VK_FORMAT_R8_SINT),
	MKFN(VK_FORMAT_R8_SRGB),
	MKFN(VK_FORMAT_R8G8_UNORM),
	MKFN(VK_FORMAT_R8G8_SNORM),
	MKFN(VK_FORMAT_R8G8_USCALED),
	MKFN(VK_FORMAT_R8G8_SSCALED),
	MKFN(VK_FORMAT_R8G8_UINT),
	MKFN(VK_FORMAT_R8G8_SINT),
	MKFN(VK_FORMAT_R8G8_SRGB),
	MKFN(VK_FORMAT_R8G8B8_UNORM),
	MKFN(VK_FORMAT_R8G8B8_SNORM),
	MKFN(VK_FORMAT_R8G8B8_USCALED),
	MKFN(VK_FORMAT_R8G8B8_SSCALED),
	MKFN(VK_FORMAT_R8G8B8_UINT),
	MKFN(VK_FORMAT_R8G8B8_SINT),
	MKFN(VK_FORMAT_R8G8B8_SRGB),
	MKFN(VK_FORMAT_B8G8R8_UNORM),
	MKFN(VK_FORMAT_B8G8R8_SNORM),
	MKFN(VK_FORMAT_B8G8R8_USCALED),
	MKFN(VK_FORMAT_B8G8R8_SSCALED),
	MKFN(VK_FORMAT_B8G8R8_UINT),
	MKFN(VK_FORMAT_B8G8R8_SINT),
	MKFN(VK_FORMAT_B8G8R8_SRGB),
	MKFN(VK_FORMAT_R8G8B8A8_UNORM),
	MKFN(VK_FORMAT_R8G8B8A8_SNORM),
	MKFN(VK_FORMAT_R8G8B8A8_USCALED),
	MKFN(VK_FORMAT_R8G8B8A8_SSCALED),
	MKFN(VK_FORMAT_R8G8B8A8_UINT),
	MKFN(VK_FORMAT_R8G8B8A8_SINT),
	MKFN(VK_FORMAT_R8G8B8A8_SRGB),
	MKFN(VK_FORMAT_B8G8R8A8_UNORM),
	MKFN(VK_FORMAT_B8G8R8A8_SNORM),
	MKFN(VK_FORMAT_B8G8R8A8_USCALED),
	MKFN(VK_FORMAT_B8G8R8A8_SSCALED),
	MKFN(VK_FORMAT_B8G8R8A8_UINT),
	MKFN(VK_FORMAT_B8G8R8A8_SINT),
	MKFN(VK_FORMAT_B8G8R8A8_SRGB),
	MKFN(VK_FORMAT_A8B8G8R8_UNORM_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_SNORM_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_USCALED_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_SSCALED_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_UINT_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_SINT_PACK32),
	MKFN(VK_FORMAT_A8B8G8R8_SRGB_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_UNORM_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_SNORM_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_USCALED_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_SSCALED_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_UINT_PACK32),
	MKFN(VK_FORMAT_A2R10G10B10_SINT_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_UNORM_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_SNORM_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_USCALED_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_SSCALED_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_UINT_PACK32),
	MKFN(VK_FORMAT_A2B10G10R10_SINT_PACK32),
	MKFN(VK_FORMAT_R16_UNORM),
	MKFN(VK_FORMAT_R16_SNORM),
	MKFN(VK_FORMAT_R16_USCALED),
	MKFN(VK_FORMAT_R16_SSCALED),
	MKFN(VK_FORMAT_R16_UINT),
	MKFN(VK_FORMAT_R16_SINT),
	MKFN(VK_FORMAT_R16_SFLOAT),
	MKFN(VK_FORMAT_R16G16_UNORM),
	MKFN(VK_FORMAT_R16G16_SNORM),
	MKFN(VK_FORMAT_R16G16_USCALED),
	MKFN(VK_FORMAT_R16G16_SSCALED),
	MKFN(VK_FORMAT_R16G16_UINT),
	MKFN(VK_FORMAT_R16G16_SINT),
	MKFN(VK_FORMAT_R16G16_SFLOAT),
	MKFN(VK_FORMAT_R16G16B16_UNORM),
	MKFN(VK_FORMAT_R16G16B16_SNORM),
	MKFN(VK_FORMAT_R16G16B16_USCALED),
	MKFN(VK_FORMAT_R16G16B16_SSCALED),
	MKFN(VK_FORMAT_R16G16B16_UINT),
	MKFN(VK_FORMAT_R16G16B16_SINT),
	MKFN(VK_FORMAT_R16G16B16_SFLOAT),
	MKFN(VK_FORMAT_R16G16B16A16_UNORM),
	MKFN(VK_FORMAT_R16G16B16A16_SNORM),
	MKFN(VK_FORMAT_R16G16B16A16_USCALED),
	MKFN(VK_FORMAT_R16G16B16A16_SSCALED),
	MKFN(VK_FORMAT_R16G16B16A16_UINT),
	MKFN(VK_FORMAT_R16G16B16A16_SINT),
	MKFN(VK_FORMAT_R16G16B16A16_SFLOAT),
	MKFN(VK_FORMAT_R32_UINT),
	MKFN(VK_FORMAT_R32_SINT),
	MKFN(VK_FORMAT_R32_SFLOAT),
	MKFN(VK_FORMAT_R32G32_UINT),
	MKFN(VK_FORMAT_R32G32_SINT),
	MKFN(VK_FORMAT_R32G32_SFLOAT),
	MKFN(VK_FORMAT_R32G32B32_UINT),
	MKFN(VK_FORMAT_R32G32B32_SINT),
	MKFN(VK_FORMAT_R32G32B32_SFLOAT),
	MKFN(VK_FORMAT_R32G32B32A32_UINT),
	MKFN(VK_FORMAT_R32G32B32A32_SINT),
	MKFN(VK_FORMAT_R32G32B32A32_SFLOAT),
	MKFN(VK_FORMAT_R64_UINT),
	MKFN(VK_FORMAT_R64_SINT),
	MKFN(VK_FORMAT_R64_SFLOAT),
	MKFN(VK_FORMAT_R64G64_UINT),
	MKFN(VK_FORMAT_R64G64_SINT),
	MKFN(VK_FORMAT_R64G64_SFLOAT),
	MKFN(VK_FORMAT_R64G64B64_UINT),
	MKFN(VK_FORMAT_R64G64B64_SINT),
	MKFN(VK_FORMAT_R64G64B64_SFLOAT),
	MKFN(VK_FORMAT_R64G64B64A64_UINT),
	MKFN(VK_FORMAT_R64G64B64A64_SINT),
	MKFN(VK_FORMAT_R64G64B64A64_SFLOAT),
	MKFN(VK_FORMAT_B10G11R11_UFLOAT_PACK32),
};

enum ParseStates
{
	None,
	Component,
	Number,
	Complete
};

void computePixelByteSize (ZFormatInfo& info)
{
	//if (info.integral || info.floating)
	//{
	//	info.pixelByteSize = info.componentCount * 4;
	//}
	//else
	//{
		uint32_t bitsSize = 0;
		for (int i = 0; i < 4; ++i)
		{
			bitsSize += info.componentSizes[i];
		}
		bitsSize = ROUNDUP(bitsSize, 8);
		info.pixelByteSize = bitsSize / 8;
	//}
}

void parseType (const char* const name, int& pos, ZFormatInfo& info)
{
	size_t skip = 0;
	BEGIN_SWITCH_STR(p, q, &name[pos])
		CASE_STR(p, "SNORM")
			skip = std::strlen(q);
			info.isSigned	= true;
			info.normalized	= true;
			info.scaled		= false;
			info.sized		= false;
			info.floating	= false;
			info.integral	= false;
		CASE_STR(p, "UNORM")
			skip = std::strlen(q);
			info.isSigned	= false;
			info.normalized	= true;
			info.scaled		= false;
			info.sized		= false;
			info.floating	= false;
			info.integral	= false;
		CASE_STR(p, "SSCALED")
			skip = std::strlen(q);
			info.isSigned	= true;
			info.normalized	= false;
			info.scaled		= true;
			info.sized		= false;
			info.floating	= false;
			info.integral	= false;
		CASE_STR(p, "USCALED")
			skip = std::strlen(q);
			info.isSigned	= false;
			info.normalized	= false;
			info.scaled		= true;
			info.sized		= false;
			info.floating	= false;
			info.integral	= false;
		CASE_STR(p, "SRGB")
			skip = std::strlen(q);
			info.isSigned	= true;
			info.normalized	= false;
			info.scaled		= false;
			info.sized		= false;
			info.floating	= false;
			info.integral	= false;
		CASE_STR(p, "SINT")
			skip = std::strlen(q);
			info.isSigned	= true;
			info.normalized	= false;
			info.scaled		= false;
			info.sized		= true;
			info.floating	= false;
			info.integral	= true;
		CASE_STR(p, "UINT")
			skip = std::strlen(q);
			info.isSigned	= false;
			info.normalized	= false;
			info.scaled		= false;
			info.sized		= true;
			info.floating	= false;
			info.integral	= true;
		CASE_STR(p, "SFLOAT")
			skip = std::strlen(q);
			info.isSigned	= true;
			info.normalized	= false;
			info.scaled		= false;
			info.sized		= true;
			info.floating	= true;
			info.integral	= false;
		CASE_STR(p, "UFLOAT")
			skip = std::strlen(q);
			info.isSigned	= false;
			info.normalized	= false;
			info.scaled		= false;
			info.sized		= true;
			info.floating	= true;
			info.integral	= false;
		CASE_STR_DEFAULT(p)
			ASSERTMSG(0, "Format not implemented");
	END_SWITCH_STR(p)
	pos += concise_convert(skip, pos);
}

uint8_t parseNumber (const char* const name, int& pos)
{
	int num[2];
	int mag= 0;
	while (std::isdigit(name[pos]))
	{
		num[mag++] = name[pos++] - '0';
	}
	uint8_t res = 0u;
	uint8_t mul = 1u;
	while (--mag >= 0)
	{
		res = uint8_t(res + num[mag] * mul);
		mul = uint8_t(mul * 10);
	}
	return res;
}

bool parseComponents (const char* const name, int& pos, int& index, int& comp, ZFormatInfo& info)
{
	uint8_t width = 0;

	switch (name[pos])
	{
	case '\0':									return false;
	case '_':	pos += 1;						return false;
	case 'R':	pos += 1;	comp = 0;			break;
	case 'G':	pos += 1;	comp = 1;			break;
	case 'B':	pos += 1;	comp = 2;			break;
	case 'A':	pos += 1;	comp = 3;			break;
	default:	width = parseNumber(name, pos);	break;
	}

	if (width)
	{
		info.swizzling[index++] = int8_t(comp);
		info.componentSizes[comp] = width;
	}

	return true;
}

void parseComponents (const char* const name, int& pos, ZFormatInfo& info)
{
	info.swizzling[0]
	= info.swizzling[1]
	= info.swizzling[2]
	= info.swizzling[3] = -1;

	info.componentSizes[0]
	= info.componentSizes[1]
	= info.componentSizes[2]
	= info.componentSizes[3] = 0;

	int			comp	= 0;
	int			index	= 0;

	while (parseComponents(name, pos, index, comp, info))
		{ /* do nothing */ }

	info.componentCount = static_cast<uint32_t>(index);

	{
		int8_t swizzling[4];
		for (index = 0; index < 4; ++index)
		{
			swizzling[index] = info.swizzling[index];
			info.swizzling[index] = (-1);
		}
		for (index = 0; index < 4; ++index)
		{
			comp = swizzling[index];
			if (comp >= 0)
				info.swizzling[comp] = int8_t(index);
		}
	}
}

ZFormatInfo makeFormatInfo (const FormatAndName* const pFan)
{
	ZFormatInfo	res;
	res.format	= pFan->format;
	res.name	= pFan->name;

	int pos		= 0; // skip "VK_FORMAT_"
	parseComponents(&pFan->name[10], pos, res);
	parseType(&pFan->name[10], pos, res);
	computePixelByteSize(res);

	return res;
}

const FormatAndName* findFormatAndName (VkFormat format)
{
	const FormatAndName* pFan = nullptr;
	for (const FormatAndName& fan : formatAndNames) {
		if (fan.format == format) {
			pFan = &fan;
			break;
		}
	}
	return pFan;
}

} // unnamed namespace

namespace vtf
{

const char* getFormatString(VkFormat format)
{
	const FormatAndName* pFan = findFormatAndName(format);
	return pFan ? pFan->name : nullptr;
}

ZFormatInfo	getFormatInfo (VkFormat format)
{
	const FormatAndName* pFan = findFormatAndName(format);
	ASSERTMSG(pFan, "Format not implemented");
	if (nullptr == pFan) {
		ZFormatInfo	res{};
		res.format	= VK_FORMAT_UNDEFINED;
		return res;
	}
	return makeFormatInfo(pFan);
}

VkComponentMapping makeComponentMapping (const ZFormatInfo& info)
{
	const VkComponentSwizzle swizzling[]
	{
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A
	};
	VkComponentMapping res{};
	res.r	= info.swizzling[0] >= 0 ? swizzling[info.swizzling[0]] : VK_COMPONENT_SWIZZLE_ZERO;
	res.g	= info.swizzling[1] >= 0 ? swizzling[info.swizzling[1]] : VK_COMPONENT_SWIZZLE_ZERO;
	res.b	= info.swizzling[2] >= 0 ? swizzling[info.swizzling[2]] : VK_COMPONENT_SWIZZLE_ZERO;
	res.a	= info.swizzling[3] >= 0 ? swizzling[info.swizzling[3]] : VK_COMPONENT_SWIZZLE_ZERO;
	return res;
}

} // vtf


