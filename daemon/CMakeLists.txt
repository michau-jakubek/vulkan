# daemon project

add_definitions(-DSHARED_LIBRARY=1)

set(daemon_files
        daemon.cpp
        daemon.hpp)
add_library(daemon SHARED ${daemon_files})
target_include_directories(daemon BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_link_libraries(daemon framework console ${COMMON_LIBS})
