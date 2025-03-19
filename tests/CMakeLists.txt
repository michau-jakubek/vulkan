if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "TESTS: ${ASSETS_PATH} not defined")
endif()

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
	nothingCompute.cpp
	nothingCompute.hpp
    intGraphics.cpp
    intGraphics.hpp
    intCipher.cpp
    intCipher.hpp
    intThreadPool.cpp
    intThreadPool.hpp
    intSynchronization2.cpp
    intSynchronization2.hpp
    ${daemon_test_files}
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
    shaderObjectTriangleTests.cpp
    shaderObjectTriangleTests.hpp
    shaderObjectComputeTests.cpp
    shaderObjectComputeTests.hpp
	descriptorBufferTests.cpp
	descriptorBufferTests.hpp
	intGeomTests.cpp
	intGeomTests.hpp
	cogWheelsTests.cpp
	cogWheelsTests.hpp
    )

if(ENABLE_DAEMON_TEST)
list(APPEND tests_files daemonTests.cpp daemonTests.hpp)
set(daemon_header_files ${daemon_headers})
endif()

set(tests_libs
    console
    framework
	driver)

add_definitions(-DASSETS_PATH="${ASSETS_PATH}")

add_library(tests STATIC ${tests_files})
target_include_directories(tests
                           BEFORE PRIVATE ${GLFW3_INCLUDE_DIR}
                           BEFORE PRIVATE ${VULKAN_INCLUDE_DIR}
                           AFTER PRIVATE  ${console_headers}
                           AFTER PRIVATE  ${daemon_header_files}
                           AFTER PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR}
                           )
target_link_libraries(tests pdriver driver ${tests_libs})

