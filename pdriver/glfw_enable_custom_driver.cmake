
file(GLOB_RECURSE glfw_header_files "${GLFW3_INCLUDE_DIR}/*.h" "${GLFW3_INCLUDE_DIR}/*.hpp")

set(PATTERN "glfwInitVulkanLoader")
set(GLFW_ENABLE_CUSTOM_DRIVER FALSE)
get_filename_component(CURRENT_MODULE_FILE ${CMAKE_CURRENT_LIST_FILE} NAME)

foreach(file ${glfw_header_files})

    #message(STATUS "process file: ${file}")

    if(GLFW_ENABLE_CUSTOM_DRIVER)
        break()
    endif()

    file(READ "${file}" file_content)

    string(REGEX MATCH ".*${PATTERN}.*" match "${file_content}")

    if(match)
        get_filename_component(filename ${file} NAME)
        message(STATUS "${CURRENT_MODULE_FILE} found ${PATTERN} in ${filename}")
	set(GLFW_ENABLE_CUSTOM_DRIVER TRUE)
    endif()
endforeach()

