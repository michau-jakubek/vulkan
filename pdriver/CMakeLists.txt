if (SYSTEM_OS_WINDOWS EQUAL 1)
    set(PLATFORM_PDRIVER_SUBDIR win)
elseif (SYSTEM_OS_LINUX EQUAL 1)
    set(PLATFORM_PDRIVER_SUBDIR lnx)
else ()
    message(FATAL_ERROR "Unsupported OS platform")
endif()

add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")

set(pdriver_files
	demangle.hpp
	demangle.cpp
    vtfPlatformDriver.hpp
    ${PLATFORM_PDRIVER_SUBDIR}/vtfPlatformDriver.cpp)

add_library(pdriver STATIC ${pdriver_files})
target_include_directories(pdriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pdriver ${COMMON_LIBS})
