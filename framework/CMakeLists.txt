set(framework_files
        demangle.hpp
        vtfFilesystem.hpp
        vtfZDeletable.cpp
        vtfZDeletable.hpp
        vtfBacktrace.cpp
        vtfBacktrace.hpp
        vtfZUtils.cpp
        vtfZUtils.hpp
        vtfCUtils.cpp
        vtfCUtils.hpp
        vtfVkUtils.hpp
        vtfVkUtils.cpp
        vtfFormatUtils.cpp
        vtfFormatUtils.hpp
        vtfVector.hpp
        vtfZBuffer.hpp
        vtfZCommandBuffer.hpp
        vtfZDeviceMemory.hpp
        vtfProgramCollection.cpp
        vtfProgramCollection.hpp
        vtfDebugMessenger.cpp
        vtfDebugMessenger.hpp
        vtfPipelineLayout.cpp
        vtfPipelineLayout.hpp
        vtfVertexInput.cpp
        vtfVertexInput.hpp
        vtfObjectLoader.cpp
        vtfObjectLoader.hpp
        vtfGlfwEvents.cpp
        vtfGlfwEvents.hpp
        vtfContext.cpp
        vtfContext.hpp
        vtfCanvas.cpp
        vtfCanvas.hpp
)

add_library(framework STATIC ${framework_files})
target_include_directories(framework BEFORE PRIVATE ${VULKAN_INCLUDE_PATH})
target_link_libraries(framework ${COMMON_LIBS})

