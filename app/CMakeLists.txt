# app project

if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "APP: ${ASSETS_PATH} not defined")
endif()

add_definitions(-DTHIS_EXECUTABLE_NAME="app")
message(STATUS "Create definition: -DTHIS_EXECUTABLE_NAME=\"app\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Create definition: -DASSETS_PATH=${ASSETS_PATH}")

set(app_files
        main.cpp
        main.hpp)
add_executable(app ${app_files})
target_include_directories(app BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_link_libraries(app ${app_libs} daemon)
