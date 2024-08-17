if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "TESTS: ${ASSETS_PATH} not defined")
endif()

set(tests_libs
    ${COMMON_LIBS}
    console
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
    intGraphics.cpp
    intGraphics.hpp
    intCipher.cpp
    intCipher.hpp
    intThreadPool.cpp
    intThreadPool.hpp
    intSynchronization2.cpp
    intSynchronization2.hpp
    daemonTests.cpp
    daemonTests.hpp
    subgroupMatrixTests.cpp
    subgroupMatrixTests.hpp
    topologyTests.cpp
    topologyTests.hpp
    demoteInvocationsTests.cpp
    demoteInvocationsTests.hpp
    blendingTests.cpp
    blendingTests.hpp
    sparseBufferTests.cpp
    sparseBufferTests.hpp
    deviceTimeoutTests.cpp
    deviceTimeoutTests.hpp
    )

add_definitions(-DASSETS_PATH="${ASSETS_PATH}")

add_library(tests STATIC ${tests_files})
target_include_directories(tests
                           BEFORE PRIVATE ${GLFW3_INCLUDE_DIR}
			   BEFORE PRIVATE ${VULKAN_INCLUDE_DIR}
                           AFTER PRIVATE  ${console_headers}
                           AFTER PRIVATE  ${daemon_headers}
                           AFTER PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR}
                           )
target_link_libraries(tests ${tests_libs})
