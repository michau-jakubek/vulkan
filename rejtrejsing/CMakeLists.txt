set(rt_files
	vtfRTTypes.cpp
	vtfRTTypes.hpp
	vtfRTLayoutManager.cpp
	vtfRTLayoutManager.hpp
	vtfRTShaderCollection.cpp
	vtfRTShaderCollection.hpp
	vtfRTPipeline.cpp
	vtfRTPipeline.hpp
	vtfRTUnitTests.cpp
)

set(rt_libs console framework driver)

add_library(rejtrejsing STATIC ${rt_files})

target_include_directories(rejtrejsing
                           BEFORE PRIVATE ${GLFW3_INCLUDE_DIR}
                           BEFORE PRIVATE ${VULKAN_INCLUDE_DIR}
                           AFTER PRIVATE  ${console_headers}
                           AFTER PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR}
                           )
target_link_libraries(rejtrejsing pdriver driver ${rt_libs})

