# daemon project

add_definitions(-DSHARED_LIBRARY=1)

set(daemon_files
        daemon.cpp
        daemon.hpp)
set(daemon_libs
		framework
		console
		driver)

add_library(daemon SHARED ${daemon_files})
target_include_directories(daemon BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(daemon BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_link_libraries(daemon ${daemon_libs})

if (${DETECTED_COMPILER} EQUAL 3)
    set_property(TARGET daemon PROPERTY LINK_FLAGS "/DELAYLOAD:${VULKAN_DRIVER}")
    target_link_libraries(daemon ${daemon_libs} delayimp)
endif()

# Optionally for all executables
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:mylibrary.dll"
