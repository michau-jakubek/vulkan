# vtf project

if(NOT EXISTS ${ASSETS_PATH})
	message(FATAL_ERROR "VTF: ${ASSETS_PATH} not defined")
endif()

set(VTF_GIT_VERSION_FILE_NAME "vtf_git_version.inl")
set(VTF_GIT_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/${VTF_GIT_VERSION_FILE_NAME})
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

add_definitions(-DTHIS_EXECUTABLE_NAME="vtf")
message(STATUS "Define preprocessor constant: THIS_EXECUTABLE_NAME=\"vtf\"")
add_definitions(-DASSETS_PATH="${ASSETS_PATH}")
message(STATUS "Define preprocessor constant: ASSETS_PATH=${ASSETS_PATH}")
add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")
message(STATUS "Define preprocessor constant: VULKAN_DRIVER=${VULKAN_DRIVER}")

if(VTF_AS_DLL)
	set(vtf_app_hpp vtf_app.cpp vtf_app.hpp)
endif()
set(vtf_files
        main.cpp
        main.hpp
		${VTF_GIT_VERSION_FILE}
		inlineParams.cpp
		${vtf_app_hpp})

set(vtf_libs
	    pdriver
        framework
        console
        tests
        driver)

if(ENABLE_DAEMON_TEST)
list(APPEND vtf_libs daemon)
endif()

if(VTF_AS_DLL)
    add_library(vtf SHARED ${vtf_files})
else()
    add_executable(vtf ${vtf_files})
endif()
target_include_directories(vtf BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(vtf BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(vtf BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_link_libraries(vtf ${vtf_libs})
add_dependencies(vtf vtf_git_version)

if (${DETECTED_COMPILER} EQUAL ${DETECTED_COMPILER_MSVC})
    set_property(TARGET vtf PROPERTY LINK_FLAGS "/DELAYLOAD:${VULKAN_DRIVER}")
    target_link_libraries(vtf ${vtf_libs} delayimp)
endif()

