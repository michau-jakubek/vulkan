if(NOT EXISTS ${ASSETS_PATH})
    message(FATAL_ERROR "TESTS: ${ASSETS_PATH} not defined")
endif()

if(VTF_AS_DLL)
    set(vtf_app_hpp ../app/vtf_app.hpp ../app/vtf_app.cpp vtfLauncher.hpp)
    if(SYSTEM_OS_WINDOWS)
	    list(APPEND vtf_app_hpp vtfLauncherWin.cpp)
    else()
	    list(APPEND vtf_app_hpp vtfLauncherLin.cpp)
    endif()
else()
    set(vtf_app_hpp ../app/vtf_app.hpp)
endif()

if(DESCRIPTOR_HEAP_AVAILABLE)
    set(descriptorHeapTests_files descriptorHeapTests.cpp descriptorHeapTests.hpp)
endif()

check_vtf_libs_version_greater_equal("1.0.0.0" VTF_LIBS_VERSION_1_0_0)
check_vtf_libs_version_greater_equal("1.0.1.0" VTF_LIBS_VERSION_1_0_1)

if(VTF_LIBS_VERSION_1_0_1)
    set(rayTracingMicromapTests_files rayTracingMicromapTests.cpp rayTracingMicromapTests.hpp)
endif()

set(tests_files
    allTests.cpp
    allTests.hpp
	${vtf_app_hpp}
	triangleTests.cpp
	triangleTests.hpp
    fractalsTests.cpp
    fractalsTests.hpp
	#viewerTests.cpp
	#viewerTests.hpp
    lineWidthTests.cpp
    lineWidthTests.hpp
	#multiViewTests.cpp
	#multiViewTests.hpp
	nothingComputeTests.cpp
	nothingComputeTests.hpp
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
	${descriptorHeapTests_files}
	cogwheelsTests.cpp
	cogwheelsTests.hpp
	cooperativeMatrixTests.cpp
	cooperativeMatrixTests.hpp
	taskMeshTriangleTests.cpp
	taskMeshTriangleTests.hpp
	banalDRLRTests.cpp
	banalDRLRTests.hpp
	simpleDRLRTests.cpp
	simpleDRLRTests.hpp
	banalRenderpassTests.cpp
	banalRenderpassTests.hpp
	mutableDescriptorTests.cpp
	mutableDescriptorTests.hpp
	variablePointersTests.cpp
	variablePointersTests.hpp
	#transformFeedbackTests.cpp
	#transformFeedbackTests.hpp
	depthTests.cpp
	depthTests.hpp
	structGeneratorTests.cpp
	structGeneratorTests.hpp
	rejtrejsingIntroTests.cpp
	rejtrejsingIntroTests.hpp
    ${rayTracingMicromapTests_files}
	deviceCrashTests.cpp
	deviceCrashTests.hpp
	suzanneGltfTests.cpp
	suzanneGltfTests.hpp
	smileAITests.cpp
	smileAITests.hpp
    )

set(tests_libs
    console
    framework
	renderdoc
	rejtrejsing
	driver)

add_definitions(-DASSETS_PATH="${ASSETS_PATH}")

add_library(tests STATIC ${tests_files})
target_include_directories(tests
                           BEFORE PRIVATE ${GLFW3_INCLUDE_DIR}
                           BEFORE PRIVATE ${VULKAN_INCLUDE_DIR}
                           AFTER PRIVATE  ${console_headers}
                           AFTER PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR}
						   AFTER PRIVATE  ../app
                           )
target_link_libraries(tests pdriver driver ${tests_libs})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateTestsInl.cmake)
setup_tests_inl_generation(tests ${VTF_AS_DLL} "${tests_files}")


