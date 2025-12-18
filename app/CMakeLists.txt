# app project

if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "APP: ${ASSETS_PATH} not defined")
endif()

set(VTF_GIT_VERSION_FILE_NAME "vtf_git_version.inl")
set(VTF_GIT_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/${VTF_GIT_VERSION_FILE_NAME})
execute_process(
	COMMAND ${CMAKE_COMMAND} -E echo "#define VTF_GIT_VERSION <unknown>"
	OUTPUT_FILE ${VTF_GIT_VERSION_FILE}
)
set(GIT_HASH "")
add_custom_command(
	OUTPUT ${VTF_GIT_VERSION_FILE}
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vtf_git_version.py ${VTF_GIT_VERSION_FILE}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating ${VTF_GIT_VERSION_FILE_NAME}"
)
add_custom_target(vtf_git_version ALL DEPENDS ${VTF_GIT_VERSION_FILE})

add_definitions(-DTHIS_EXECUTABLE_NAME="app")
message(STATUS "Define preprocessor constant: THIS_EXECUTABLE_NAME=\"app\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Define preprocessor constant: ASSETS_PATH=${ASSETS_PATH}")
add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")
message(STATUS "Define preprocessor constant: VULKAN_DRIVER=${VULKAN_DRIVER}")

set(app_files
        main.cpp
        main.hpp
		${VTF_GIT_VERSION_FILE}
		inlineParams.cpp)

set(app_libs
	    pdriver
        framework
        console
        tests
        driver)

if(ENABLE_DAEMON_TEST)
list(APPEND app_libs daemon)
endif()


add_executable(app ${app_files})
target_include_directories(app BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(app BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(app BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_link_libraries(app ${app_libs})
add_dependencies(app vtf_git_version)

if (${DETECTED_COMPILER} EQUAL ${DETECTED_COMPILER_MSVC})
    set_property(TARGET app PROPERTY LINK_FLAGS "/DELAYLOAD:${VULKAN_DRIVER}")
    target_link_libraries(app ${app_libs} delayimp)
endif()

