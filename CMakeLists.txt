cmake_minimum_required(VERSION 3.10.2)

project(vulkan-trivial-framework CXX)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
    set(SYSTEM_OS_LINUX 1)
    set(SYSTEM_OS_WINDOWS 0)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
    set(SYSTEM_OS_LINUX 0)
    set(SYSTEM_OS_WINDOWS 1)
endif()
add_definitions(-DSYSTEM_OS_LINUX=${SYSTEM_OS_LINUX})
add_definitions(-DSYSTEM_OS_WINDOWS=${SYSTEM_OS_WINDOWS})
message(STATUS "Defined preprocessor constant: SYSTEM_OS_LINUX=${SYSTEM_OS_LINUX}")
message(STATUS "Defined preprocessor constant: SYSTEM_OS_WINDOWS=${SYSTEM_OS_WINDOWS}")

set(CMAKE_CXX_STANDARD 17)

if (NOT WIN32)
	set(FPIC_FLAG "-fPIC")
endif()

if     (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_definitions(-DDETECTED_COMPILER=1)
	set(DETECTED_COMPILER 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	add_definitions(-DDETECTED_COMPILER=2)
	set(DETECTED_COMPILER 2)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	add_definitions(-DDETECTED_COMPILER=3)
	set(DETECTED_COMPILER 3)
else   ()
	add_definitions(-DDETECTED_COMPILER=0)
	set(DETECTED_COMPILER 0)
endif  ()
message(STATUS "Defined preprocessor constant: DETECTED_COMPILER=${DETECTED_COMPILER}")

string(TOUPPER ${CMAKE_GENERATOR} UPPER_CASE_CMAKE_GENERATOR)
string(REGEX MATCH "VISUAL" CURRENT_GENERATOR ${UPPER_CASE_CMAKE_GENERATOR})
if ("${CURRENT_GENERATOR}" STREQUAL "VISUAL")
    # Globally disable terribly onerous warning C26812: Prefer 'enum class' over 'enum' (Enum.3)
	set(COMMON_BUILD_OPTIONS "/std:c++17 /W4 /GR /wd26812 /MP")
    set(ADDITIONAL_DEBUG_BUILD_OPTIONS "")
    set(ADDITIONAL_RELEASE_BUILD_OPTIONS "")
    set(ADDITIONAL_LIBS "")
else ()
	set(COMMON_BUILD_OPTIONS "-std=c++17 -Wall -Wextra -Wno-long-long -Wconversion -Wpedantic ${FPIC_FLAG}")
    set(ADDITIONAL_DEBUG_BUILD_OPTIONS "-ggdb -O0")
    set(ADDITIONAL_RELEASE_BUILD_OPTIONS "-O2")
    if (${SYSTEM_OS_LINUX} EQUAL 0)
        set(ADDITIONAL_LIBS "-pthread")
    else ()
        set(ADDITIONAL_LIBS "-pthread -ldl -lm -lstdc++fs -lX11")
    endif ()
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
set(VULKAN_INCLUDE_DIR "" CACHE STRING "Directory with Vulkan includes")
set(VULKAN_LIB_PATH    "" CACHE STRING "Full path to Vulkan link library")
set(GLFW3_INCLUDE_DIR  "" CACHE STRING "Directory with GLFW v3 includes")
set(GLFW3_LIB_PATH     "" CACHE STRING "Full path to GLFW v3 link library")

# Unfortunately CMAKE_SURRENT_SOURCE_DIR inside of 'cmake' subdirectory doesn't point to 'cmake' directory :(
set(PROJECT_MODULES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_MODULES_PATH})
find_package(Git REQUIRED)
find_package(Python REQUIRED)
find_package(LookForDependencies REQUIRED)
message(STATUS "Found Python interpreter: " ${Python_EXECUTABLE})

if ("${VULKAN_LIB_PATH}" STREQUAL "")
	if (NOT "${LOOK_FOR_VULKAN}" STREQUAL "")
		get_filename_component(ABS_LOOK_FOR_VULKAN ${LOOK_FOR_VULKAN} REALPATH)
		find_vulkan("${ABS_LOOK_FOR_VULKAN}" "${HINT_FOR_VULKAN}" Q_VULKAN_INCLUDE_DIR Q_VULKAN_LIB_PATH)
		string(REPLACE "\"" "" VULKAN_LIB_PATH    "${Q_VULKAN_LIB_PATH}")
	endif()
endif()
if ("${VULKAN_INCLUDE_DIR}" STREQUAL "")
	string(REPLACE "\"" "" VULKAN_INCLUDE_DIR "${Q_VULKAN_INCLUDE_DIR}")
endif()

if ("${GLFW3_LIB_PATH}" STREQUAL "")
	if (NOT "${LOOK_FOR_GLFW3}" STREQUAL "")
		get_filename_component(ABS_LOOK_FOR_GLFW3 ${LOOK_FOR_GLFW3} REALPATH)
		find_glfw3( "${ABS_LOOK_FOR_GLFW3}"  "${HINT_FOR_GLFW3}" Q_GLFW3_INCLUDE_DIR Q_GLFW3_LIB_PATH)
		string(REPLACE "\"" "" GLFW3_LIB_PATH     "${Q_GLFW3_LIB_PATH}")
	endif()
endif()
if ("${GLFW3_INCLUDE_DIR}" STREQUAL "")
	string(REPLACE "\"" "" GLFW3_INCLUDE_DIR  "${Q_GLFW3_INCLUDE_DIR}")
endif()

if("${VULKAN_LIB_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
		message(WARNING "VULKAN_LIB_PATH not defined, try for an example: c:\\VulkanSDK\\1.3.216.0\\Lib\\vulkan-1.lib")
    else()
		message(WARNING "VULKAN_LIB_PATH not defined, try for an example: /opt/vulkan/1.3.216.0/x86_64/lib/libvulkan.so")
    endif()
    message(WARNING "You can also try -DLOOK_FOR_VULKAN and -DHINT_FOR_VULKAN if it is ambigous, "
                    "example: -DLOOK_FOR_VULKAN=/c/VulkanSDK -DHONT_FOR_VULKAN=1.3.250.1,Lib32")
    message(FATAL_ERROR "Looking for Vulkan dependencies failed.")
endif()
message(STATUS "Define variable VULKAN_LIB_PATH=${VULKAN_LIB_PATH}")

if ("${VULKAN_DRIVER}" STREQUAL "")
	if (WIN32)
		set(VULKAN_DRIVER "vulkan-1.dll")
	else()
		get_filename_component(VULKAN_DRIVER ${VULKAN_LIB_PATH} NAME)
	endif()
endif()

if("${VULKAN_INCLUDE_DIR}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(WARNING "VULKAN_INCLUDE_DIR not defined, try for an example: c:\\VulkanSDK\\1.3.216.0\\Include")
    else()
        message(WARNING "VULKAN_INCLUDE_DIR not defined, try for an example: /opt/vulkan/1.3.216.0/x86_64/include")
    endif()
    message(WARNING "You can also try -DLOOK_FOR_VULKAN and -DHINT_FOR_VULKAN if it is ambigous, "
                    "example: -DLOOK_FOR_VULKAN=/c/VulkanSDK -DHONT_FOR_VULKAN=1.3.250.1,Lib32")
    message(FATAL_ERROR "Looking for Vulkan dependencies failed.")
endif()
message(STATUS "Define variable VULKAN_INCLUDE_DIR=${VULKAN_INCLUDE_DIR}")


if("${GLFW3_LIB_PATH}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
        message(WARNING "GLFW3_LIB_PATH not defined, try for an example: c:\\glfw-3.3.6.bin.WIN64\\lib-vc2019\\glfw3.lib")
    else()
        message(WARNING "GLFW3_LIB_PATH not defined, try for an example: /opt/glfw/build/src/libglfw3.a")
    endif()
    message(WARNING "You can also try -DLOOK_FOR_GLFW3 and -DHINT_FOR_GLFW3 if it is ambigous, "
                    "example: -DLOOK_FOR_GLFW2=/c/VulkanDeps/glfw-3.3.8.bin.WIN64/ -HINT_FOR_GLFW3=lib-vc2022")
	message(FATAL_ERROR "Looking for GLFW3 dependencied failed.")
endif()

message(STATUS "Define variable GLFW3_LIB_PATH=${GLFW3_LIB_PATH}")
if("${GLFW3_INCLUDE_DIR}" STREQUAL "")
    if(${SYSTEM_OS_LINUX} EQUAL 0)
		message(WARNING "GLFW3_INCLUDE_DIR not defined, try for an example: c:\\glfw-3.3.6.bin.WIN64\\include")
    else()
		message(WARNING "GLFW3_INCLUDE_DIR not defined, try for an example: /opt/glfw/include")
    endif()
	message(WARNING "You can also try -DLOOK_FOR_GLFW3 and -DHINT_FOR_GLFW3 if it is ambigous, "
		"example: -DLOOK_FOR_GLFW2=/c/VulkanDeps/glfw-3.3.8.bin.WIN64/ -HINT_FOR_GLFW3=lib-vc2022")
	message(FATAL_ERROR "Looking for GLFW3 dependencied failed.")
endif()
message(STATUS "Define variable GLFW3_INCLUDE_DIR=${GLFW3_INCLUDE_DIR}")


if(NOT EXISTS "${VULKAN_INCLUDE_DIR}" OR NOT EXISTS "${VULKAN_LIB_PATH}"
	OR NOT EXISTS "${GLFW3_INCLUDE_DIR}" OR NOT EXISTS "${GLFW3_LIB_PATH}")
message(FATAL_ERROR "--- Some of the following do not exist: ---\n"
		"VULKAN_INCLUDE_DIR=${VULKAN_INCLUDE_DIR}\n"
		"VULKAN_LIB_PATH=${VULKAN_LIB_PATH}\n"
		"GLFW3_INCLUDE_DIR=${GLFW3_INCLUDE_DIR}\n"
		"GLFW3_LIB_PATH=${GLFW3_LIB_PATH}\n"
		"--- Try also: ---\n"
		"LOOK_FOR_VULKAN=<Vulkan path> [HINT_FOR_VULKAN=[subdir[, ...]]]\n"
		"LOOK_FOR_GLFW3=<GLFW3 path> [HINT_FOR_GLFW3=[subdir[, ...]]]\n")
endif()

#message(STATUS "Found VULKAN_INCLUDE_DIR=${VULKAN_INCLUDE_DIR}")
#message(STATUS "Found VULKAN_LIB_PATH=${VULKAN_LIB_PATH}")
#message(STATUS "Found GLFW3_INCLUDE_DIR=${GLFW3_INCLUDE_DIR}")
#message(STATUS "Found GLFW3_LIB_PATH=${GLFW3_LIB_PATH}")

include_directories(${VULKAN_INCLUDE_DIR})
include_directories(${GLFW3_INCLUDE_DIR})

get_filename_component(ABS_VULKAN_LIB_PATH ${VULKAN_LIB_PATH} REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
get_filename_component(ABS_GLFW3_LIB_PATH ${GLFW3_LIB_PATH} REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
set(COMMON_LIBS ${ADDITIONAL_LIBS} ${ABS_VULKAN_LIB_PATH} ${ABS_GLFW3_LIB_PATH})

set(ASSETS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets)

add_subdirectory(external)
include_directories(
	external/stb
	external/stl_reader
	)

add_subdirectory(driver)
include_directories(driver)

add_subdirectory(console)
include_directories(console)
add_dependencies(console external)

add_subdirectory(framework)
include_directories(framework)
add_dependencies(framework external)

add_subdirectory(daemon)
include_directories(daemon)
add_dependencies(daemon external)

add_subdirectory(tests)
include_directories(tests)
add_dependencies(tests external)

include_directories(app)
add_subdirectory(app)

add_custom_target(copy-binaries ALL
                    DEPENDS daemon console app
                    COMMENT "Copy binaries ...")
add_custom_command(TARGET copy-binaries
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E echo
                #"Copying" $<TARGET_FILE:daemon> "to" $<TARGET_FILE_DIR:app>
                COMMAND ${CMAKE_COMMAND} ARGS -E copy
                $<TARGET_FILE:daemon> $<TARGET_FILE_DIR:app>)

