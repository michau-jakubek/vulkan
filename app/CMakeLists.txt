if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "${ASSETS_PATH} nit defined")
endif()

set(app_files main.cpp)
add_definitions(-DTHIS_EXECUTABLE_NAME="app")
message(STATUS "Create definition: -DTHIS_EXECUTABLE_NAME=\"app\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Create definition: -DASSETS_PATH=${ASSETS_PATH}")
add_executable(app ${app_files})
target_include_directories(app BEFORE PRIVATE ${VULKAN_INCLUDE_PATH})
target_link_libraries(app ${app_libs})

