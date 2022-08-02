set(tests_libs
    ${COMMON_LIBS}
    framework
    )
set(tests_files
    allTests.cpp
    allTests.hpp
    triangleTests.cpp
    triangleTests.hpp
    fractalsTests.cpp
    fractalsTests.hpp
    )

add_library(tests STATIC ${tests_files})
target_include_directories(tests BEFORE PRIVATE ${VULKAN_INCLUDE_PATH})
target_link_libraries(tests ${tests_libs})


