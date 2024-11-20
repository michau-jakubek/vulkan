#include "stb_image.hpp"

#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#if DETECTED_COMPILER == DETECTED_COMPILER_MSVC
#include "stb_image.h"
#elif DETECTED_COMPILER == DETECTED_COMPILER_GNU
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpragmas\"")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic ignored \"-Wimplicit-int-conversion\"")
#include "stb_image.h"
_Pragma("GCC diagnostic pop")
#elif DETECTED_COMPILER == DETECTED_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#include "stb_image.h"
#pragma clang diagnostic pop
#else
#error Unknown compiler. Expected MSVC, GNU or CLANG
#endif
