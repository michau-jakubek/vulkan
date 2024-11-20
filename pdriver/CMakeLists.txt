if (SYSTEM_OS_WINDOWS EQUAL 1)
    set(PLATFORM_PDRIVER_SUBDIR win)
elseif (SYSTEM_OS_LINUX EQUAL 1)
    set(PLATFORM_PDRIVER_SUBDIR lnx)
else ()
    message(FATAL_ERROR "Unsupported OS platform")
endif()

add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")

if(NOT VULKAN_CUSTOM_DRIVER)
  message(STATUS "VULKAN_CUSTOM_DRIVER is OFF")
else()
  include(glfw_enable_custom_driver.cmake)
  if(GLFW_ENABLE_CUSTOM_DRIVER)
    add_definitions(-DVULKAN_CUSTOM_DRIVER=${GLFW_ENABLE_CUSTOM_DRIVER})
    message(STATUS "Define preprocessor constant VULKAN_CUSTOM_DRIVER=${GLFW_ENABLE_CUSTOM_DRIVER}")
  else()
    message(STATUS "VULKAN_CUSTOM_DRIVER is ON but GLFW_CUSTOM_DRIVER disables it.")
  endif()
endif()

set(pdriver_files
	demangle.hpp
	demangle.cpp
        vtfPlatformDriver.hpp
        ${PLATFORM_PDRIVER_SUBDIR}/vtfPlatformDriver.cpp)

add_library(pdriver STATIC ${pdriver_files})
target_include_directories(pdriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pdriver ${COMMON_LIBS})
