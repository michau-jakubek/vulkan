#ifndef __VTF_VULKAN_HPP_INCLUDED__
#define __VTF_VULKAN_HPP_INCLUDED__

// This file changes the vulkan.hpp file compilation
// in order to get rid of some warnings an error when
// build with another platforms, e.g. gcc, clang and
// earlier MS Visual Studio versions.
// So do not include <vulkan/vulkan.hpp> directly,
// instead try to include this file.

#define VULKAN_HPP_DISABLE_ENHANCED_MODE 1
#if DETECTED_COMPILER == DETECTED_COMPILER_MSVC
#include <vulkan/vulkan.hpp>
#elif DETECTED_COMPILER == DETECTED_COMPILER_GNU
_Pragma("GCC diagnostic push")
//_Pragma("GCC diagnostic ignored \"-Wpragmas\"")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
//_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
//_Pragma("GCC diagnostic ignored \"-Wimplicit-int-conversion\"")
#include <vulkan/vulkan.hpp>
_Pragma("GCC diagnostic pop")
#elif DETECTED_COMPILER == DETECTED_COMPILER_CLANG
#pragma clang diagnostic push
//#pragma clang diagnostic ignored "-Wsign-conversion"
//#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop
#else
#error Unknown compiler. Expected MSVC, GNU or CLANG
#endif




#endif // __VTF_VULKAN_HPP_INCLUDED__
