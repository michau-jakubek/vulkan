set(driver_libs
    ${COMMON_LIBS}
    )

set(driver_files
        vtfVulkanDriver.cpp
        vtfVulkanDriver.hpp)


add_library(driver STATIC ${driver_files})
target_include_directories(driver PRIVATE ../framework)
target_link_libraries(driver pdriver)

