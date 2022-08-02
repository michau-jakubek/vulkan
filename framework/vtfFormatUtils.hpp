#ifndef __VTF_FORMAT_UTILS_HPP_INCLUDED__
#define __VTF_FORMAT_UTILS_HPP_INCLUDED__

#include "vulkan/vulkan.h"
#include <cstring>

#define BEGIN_SWITCH_STR(variable_lhs_, variable_rhs_, switch__) {	\
	const char* variable_rhs_ = nullptr;							\
	const char*& variable_rhs_ref_ = variable_rhs_;					\
	const char* variable_lhs_ = (switch__); if (false) {
#define CASE_STR(variable_lhs_, some_str_)	} else					\
	if (variable_rhs_ref_ = (some_str_);							\
		std::strncmp(variable_lhs_, variable_rhs_ref_,				\
		std::strlen(variable_rhs_ref_)) == 0) {
#define CASE_STR_DEFAULT(variable_lhs_) } else {
#define END_SWITCH_STR(variable_lhs_)	\
	} static_cast<void>(variable_lhs_); }

namespace vtf
{

struct ZFormatInfo
{
	const char*		name;
	VkFormat		format;
	int8_t			swizzling[4];		// [3210] -> ABGR
	uint8_t			componentSizes[4];	// in RGBA order
	uint32_t		componentCount;
	uint32_t		pixelByteSize;
	bool			isSigned;
	bool			normalized;
	bool			floating;
	bool			integral;
	bool			scaled;
	bool			sized;
};

const char*			getFormatString(VkFormat format);
ZFormatInfo			getFormatInfo (VkFormat format);
VkComponentMapping	makeComponentMapping (const ZFormatInfo& info);

} // vtf

#endif // __VTF_FORMAT_UTILS_HPP_INCLUDED__
