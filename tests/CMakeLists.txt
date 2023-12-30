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
    viewerTests.cpp
    viewerTests.hpp
    lineWidthTests.cpp
    lineWidthTests.hpp
    multiViewTests.cpp
    multiViewTests.hpp
    intMatrixTests.cpp
    intMatrixTests.hpp
    intCompute.cpp
    intCompute.hpp
    intCipher.cpp
    intCipher.hpp
    subgroupMatrixTests.cpp
    subgroupMatrixTests.hpp
    topologyTests.cpp
    topologyTests.hpp
    )

add_library(tests STATIC ${tests_files})
target_include_directories(tests BEFORE PRIVATE ${GLFW3_INCLUDE_PATH})
target_include_directories(tests
                           BEFORE PRIVATE ${VULKAN_INCLUDE_PATH}
                           AFTER PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                           )
target_link_libraries(tests ${tests_libs})


