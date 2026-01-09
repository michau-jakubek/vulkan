# vtf project

if(NOT EXISTS ${ASSETS_PATH})
	message(FATAL_ERROR "VTF: ${ASSETS_PATH} not defined")
endif()

set(VTF_GIT_VERSION_FILE_NAME "vtf_git_version.inl")
set(VTF_GIT_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/../app/${VTF_GIT_VERSION_FILE_NAME})
#[[
execute_process(
	COMMAND ${CMAKE_COMMAND} -E echo "#define VTF_GIT_VERSION \"<unknown>\""
	OUTPUT_FILE ${VTF_GIT_VERSION_FILE}
)
set(GIT_HASH "")
if (NOT ${Python_EXECUTABLE})
endif()
add_custom_target(vtf_git_version ALL
	COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vtf_git_version.py ${VTF_GIT_VERSION_FILE}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating ${VTF_GIT_VERSION_FILE_NAME}"
)
]]
add_definitions(-DTHIS_EXECUTABLE_NAME="vtf-launcher")
message(STATUS "Define preprocessor constant: THIS_EXECUTABLE_NAME=\"vtf-launcher\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Define preprocessor constant: ASSETS_PATH=${ASSETS_PATH}")
add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")
message(STATUS "Define preprocessor constant: VULKAN_DRIVER=${VULKAN_DRIVER}")

set(vtf_launcher_files
        ../app/main.cpp
        ../app/main.hpp
		${VTF_GIT_VERSION_FILE}
		../app/inlineParams.cpp)

set(vtf_launcher_libs
	    pdriver
        framework
        console
        tests
        driver)

if(ENABLE_DAEMON_TEST)
list(APPEND vtf_libs daemon)
endif()

if(VTF_AS_DLL)
    add_executable(vtf-launcher ${vtf_launcher_files})
	set_target_properties(vtf-launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:vtf>")
else()
	message(FATAL_ERROR "VTF_AS_DLL must be defined")
endif()

target_include_directories(vtf-launcher BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../app)
target_include_directories(vtf-launcher BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(vtf-launcher BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(vtf-launcher  BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_link_libraries(vtf-launcher ${vtf_launcher_libs})
add_dependencies(vtf-launcher vtf_git_version)

if (${DETECTED_COMPILER} EQUAL ${DETECTED_COMPILER_MSVC})
    set_property(TARGET vtf-launcher PROPERTY LINK_FLAGS "/DELAYLOAD:${VULKAN_DRIVER}")
    target_link_libraries(vtf-launcher ${vtf_libs} delayimp)
endif()

