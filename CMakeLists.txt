cmake_minimum_required(VERSION 3.10.2)

project(my_project)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
    set(SYSTEM_OS_LINUX 1)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
    set(SYSTEM_OS_LINUX 0)
endif()
message(STATUS "Create definition: -DSYSTEM_OS_LINUX=${SYSTEM_OS_LINUX}")

string(TOUPPER ${CMAKE_GENERATOR} UPPER_CASE_CMAKE_GENERATOR)
string(REGEX MATCH "VISUAL" CURRENT_GENERATOR ${UPPER_CASE_CMAKE_GENERATOR})
if ("${CURRENT_GENERATOR}" STREQUAL "VISUAL")
    # Globally disable terribly onerous warning C26812: Prefer 'enum class' over 'enum' (Enum.3)
    set(COMMON_BUILD_OPTIONS "/std:c++17 /W4 /GR /wd26812")
    set(ADDITIONAL_DEBUG_BUILD_OPTIONS "")
    set(ADDITIONAL_RELEASE_BUILD_OPTIONS "")
    set(ADDITIONAL_LIBS "")
else ()
    set(COMMON_BUILD_OPTIONS "-std=c++17 -Wall -Wextra -Wno-long-long -Wconversion -Wpedantic")
    set(ADDITIONAL_DEBUG_BUILD_OPTIONS "-ggdb -O0")
    set(ADDITIONAL_RELEASE_BUILD_OPTIONS "-O2")
    if (${SYSTEM_OS_LINUX} EQUAL 0)
        set(ADDITIONAL_LIBS "-pthread")
    else ()
        set(ADDITIONAL_LIBS "-pthread -ldl -lstdc++fs")
    endif ()
    set(SYSTEM_OS_LINUX 0)
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" THIS_BUILD_TYPE)
if("${THIS_BUILD_TYPE}" STREQUAL "DEBUG")
    message(STATUS "Setup enabled for debugging")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_BUILD_OPTIONS} ${ADDITIONAL_DEBUG_BUILD_OPTIONS}")
else()
    message(STATUS "Setup enabled for release version")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_BUILD_OPTIONS} ${ADDITIONAL_RELEASE_BUILD_OPTIONS}")
endif()

# Cached variables only for Cmake-GUI
set(VULKAN_INCLUDE_PATH "" CACHE STRING "Full path to Vulkan includes")
set(VULKAN_LIB_PATH "" CACHE STRING "Full path to Vulkan link library")
set(GLFW3_INCLUDE_PATH "" CACHE STRING "Full path to GLFW v3 includes")
set(GLFW3_LIB_PATH "" CACHE STRING "Full path to GLFW v3 link library")

if("${VULKAN_INCLUDE_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(FATAL_ERROR "VULKAN_INCLUDE_PATH not defined, try for an example c:\\VulkanSDK\\1.3.216.0\\Include")
    else()
        message(FATAL_ERROR "VULKAN_INCLUDE_PATH not defined, try for an example /opt/vulkan/1.3.216.0/x86_64/include")
    endif()
endif()

if("${VULKAN_LIB_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(FATAL_ERROR "VULKAN_LIB_PATH not defined, try for an example c:\\VulkanSDK\\1.3.216.0\\Lib\\vulkan-1.lib")
    else()
        message(FATAL_ERROR "VULKAN_LIB_PATH not defined, try for an example /opt/vulkan/1.3.216.0/x86_64/lib/libvulkan.so")
    endif()
endif()

if("${GLFW3_INCLUDE_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(FATAL_ERROR "GLFW3_INCLUDE_PATH not defined, try for an example c:\\glfw-3.3.6.bin.WIN64\\include")
    else()
        message(FATAL_ERROR "GLFW3_INCLUDE_PATH not defined, try for an example /opt/vulkan/1.2.198.1/x86_64/include")
    endif()
endif()

if("${GLFW3_LIB_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(FATAL_ERROR "GLFW3_LIB_PATH not defined, try for an example c:\\glfw-3.3.6.bin.WIN64\\lib-vc2019\\glfw3.lib")
    else()
        message(FATAL_ERROR "GLFW3_LIB_PATH not defined, try for an example /opt/vulkan/1.2.192.1/x86_64/lib/libvulkan.so")
    endif()
endif()


include_directories(${VULKAN_INCLUDE_PATH})
include_directories(${GLFW3_INCLUDE_PATH})

set(COMMON_LIBS ${VULKAN_LIB_PATH} ${GLFW3_LIB_PATH} ${ADDITIONAL_LIBS})

add_subdirectory(framework)
include_directories(framework)

add_subdirectory(tests)
include_directories(tests)

set(app_libs ${COMMON_LIBS} framework tests)

set(ASSETS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets)
add_subdirectory(app)

