
set(VERDIR ${CMAKE_CURRENT_SOURCE_DIR}/getvkhver)

#file(REMOVE_RECURSE ${VERDIR}/build)
#message(STATUS "$$$$$$$$ ${CMAKE_CXX_COMPILER}")

if(VTF_SAFETY_CRITICAL)
	set(VTF_SC 1)
else()
	set(VTF_SC 0)
endif()

set(COMP "${CMAKE_CXX_COMPILER}")
execute_process(
	COMMAND ${CMAKE_COMMAND} -B ${VERDIR}/build -S ${VERDIR} -DINC="${VULKAN_INCLUDE_DIR}" -DCMAKE_CXX_COMPILER=${COMP} -DVTF_SAFETY_CRITICAL=${VTF_SC}
	OUTPUT_QUIET
	ERROR_QUIET
	RESULT_VARIABLE CONF_RESULT
)
if (NOT CONF_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to configure getvkhver")
endif()

execute_process(
	COMMAND ${CMAKE_COMMAND} --build ${VERDIR}/build --target getvkhver
	OUTPUT_QUIET
	ERROR_QUIET
	RESULT_VARIABLE BUILD_RESULT
)
if (NOT BUILD_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to build getvkhver")
endif()

file(GLOB_RECURSE GETVKHVER_EXECUTABLE
	"${VERDIR}/*/getvkhver${CMAKE_EXECUTABLE_SUFFIX}"
)
#message(STATUS "############### ${GETVKHVER_EXECUTABLE}")
if (NOT GETVKHVER_EXECUTABLE)
	message(FATAL_ERROR "Failed to find getvkhver executable")
endif()

execute_process(
	COMMAND ${GETVKHVER_EXECUTABLE}
	OUTPUT_VARIABLE GETVKHVER_OUTPUT
	ERROR_QUIET
	RESULT_VARIABLE RUN_RESULT
)
if (NOT RUN_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to run getvkhver")
endif()
message(STATUS "Found VK_HEADER_VERSION_COMPLETE=(${GETVKHVER_OUTPUT})")

string(REGEX MATCH "([0-9]+),([0-9]+),([0-9]+),([0-9]+)" VERSION_MATCH "${GETVKHVER_OUTPUT}")
if (VERSION_MATCH)
	string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+),([0-9]+)" "\\1" VULKAN_HEADER_VERSION_VARIANT "${VERSION_MATCH}")
    string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+),([0-9]+)" "\\2" VULKAN_HEADER_VERSION_MAJOR   "${VERSION_MATCH}")
    string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+),([0-9]+)" "\\3" VULKAN_HEADER_VERSION_MINOR   "${VERSION_MATCH}")
    string(REGEX REPLACE "([0-9]+),([0-9]+),([0-9]+),([0-9]+)" "\\4" VULKAN_HEADER_VERSION_PATCH   "${VERSION_MATCH}")
else()
    message(FATAL_ERROR "Failed to parse VK_HEADER_VERSION_COMPLETE")
endif()

add_definitions(-DVULKAN_HEADER_VERSION_VARIANT=${VULKAN_HEADER_VERSION_VARIANT})
add_definitions(-DVULKAN_HEADER_VERSION_MAJOR=${VULKAN_HEADER_VERSION_MAJOR})
add_definitions(-DVULKAN_HEADER_VERSION_MINOR=${VULKAN_HEADER_VERSION_MINOR})
add_definitions(-DVULKAN_HEADER_VERSION_PATCH=${VULKAN_HEADER_VERSION_PATCH})

function(check_vulkan_version_greater_equal target_version result_var)
    set(current_vulkan_version "${VULKAN_HEADER_VERSION_MAJOR}.${VULKAN_HEADER_VERSION_MINOR}.${VULKAN_HEADER_VERSION_PATCH}.${VULKAN_HEADER_VERSION_VARIANT}")
	if(current_vulkan_version VERSION_GREATER_EQUAL target_version)
        set(${result_var} 1 PARENT_SCOPE)
    else()
        set(${result_var} 0 PARENT_SCOPE)
    endif()
endfunction()

check_vulkan_version_greater_equal("1.4.0.0" VK_VERSION_1_4_AVAILABLE)
check_vulkan_version_greater_equal("1.4.341.1" DESCRIPTOR_HEAP_AVAILABLE)
add_definitions(-DVK_VERSION_1_4_AVAILABLE=${VK_VERSION_1_4_AVAILABLE})
add_definitions(-DDESCRIPTOR_HEAP_AVAILABLE=${DESCRIPTOR_HEAP_AVAILABLE})
message(STATUS "Define preprocessor constant: VK_VERSION_1_4_AVAILABLE=${VK_VERSION_1_4_AVAILABLE}")
message(STATUS "Define preprocessor constant: DESCRIPTOR_HEAP_AVAILABLE=${DESCRIPTOR_HEAP_AVAILABLE}")

