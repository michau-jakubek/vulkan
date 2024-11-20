# app project

if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "APP: ${ASSETS_PATH} not defined")
endif()

add_definitions(-DTHIS_EXECUTABLE_NAME="app")
message(STATUS "Define preprocessor constant: THIS_EXECUTABLE_NAME=\"app\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Define preprocessor constant: ASSETS_PATH=${ASSETS_PATH}")
add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")
message(STATUS "Define preprocessor constant: VULKAN_DRIVER=${VULKAN_DRIVER}")

set(app_files
        main.cpp
        main.hpp)

set(app_libs
	pdriver
        framework
        console
        tests
        daemon
        driver)

add_executable(app ${app_files})
target_include_directories(app BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(app BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_link_libraries(app ${app_libs})

if (${DETECTED_COMPILER} EQUAL ${DETECTED_COMPILER_MSVC})
    set_property(TARGET app PROPERTY LINK_FLAGS "/DELAYLOAD:${VULKAN_DRIVER}")
    target_link_libraries(app ${app_libs} delayimp)
endif()

