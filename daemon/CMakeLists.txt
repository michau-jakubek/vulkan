# daemon project

add_definitions(-DSHARED_LIBRARY=1)

set(daemon_files
        daemon.cpp
        daemon.hpp)
add_library(daemon SHARED ${daemon_files})
target_include_directories(daemon BEFORE PRIVATE ${VULKAN_INCLUDE_PATH})
target_link_libraries(daemon ${COMMON_LIBS} console framework)
