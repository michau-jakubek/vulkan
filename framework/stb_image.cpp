#include "stb_image.hpp"

#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#ifdef _MSC_VER
#include "stb_image.h"
#else
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpragmas\"")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic ignored \"-Wimplicit-int-conversion\"")
#include "stb_image.h"
_Pragma("GCC diagnostic pop")
#endif
