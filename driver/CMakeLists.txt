set(driver_libs
    ${COMMON_LIBS}
    )

set(driver_files
        driver.cpp
        driver.hpp)

add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")

add_library(driver STATIC ${driver_files})
include_directories(../framework)
target_link_libraries(driver ${driver_libs})

