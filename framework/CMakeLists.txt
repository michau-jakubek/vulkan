set(framework_files
	demangle.hpp
	demangle.cpp
	stb_image.cpp
	stb_image.hpp
	vtfLocale.cpp
	vtfLocale.hpp
	vtfVulkan.hpp
	vtfFilesystem.hpp
	vtfZInstanceDeviceInterface.cpp
	vtfZInstanceDeviceInterface.hpp
	vtfZDeletable.cpp
	vtfZDeletable.hpp
	vtfBacktrace.cpp
	vtfBacktrace.hpp
	vtfCUtils.cpp
	vtfCUtils.hpp
	vtfStrictTemplates.hpp
	vtfVkUtils.hpp
	vtfVkUtils.cpp
	vtfZUtils.cpp
	vtfZUtils.hpp
	vtfTemplateUtils.hpp
	vtfCopyUtils.hpp
	vtfFloat16.cpp
	vtfFloat16.hpp
	vtfFormatUtils.cpp
	vtfFormatUtils.hpp
	vtfStructUtils.cpp
	vtfStructUtils.hpp
	vtfExtensions.cpp
	vtfExtensions.hpp
	vtfTermColor.cpp
	vtfTermColor.hpp
	vtfOptionParser.hpp
	vtfOptionParser.cpp
	vtfVector.hpp
	vtfMatrix.cpp
	vtfMatrix.hpp
	vtfZDeviceMemory.cpp
	vtfZDeviceMemory.hpp
	vtfThreadSafeLogger.cpp
	vtfThreadSafeLogger.hpp
	vtfThreadPool.cpp
	vtfThreadPool.hpp
	vtfThreadPoolHelpers.hpp
	vtfProgressRecorder.cpp
	vtfProgressRecorder.hpp
	vtfZBarriers.cpp
	vtfZBarriers.hpp
	vtfZBarriers2.cpp
	vtfZBarriers2.hpp
	vtfZBuffer.cpp
	vtfZBuffer.hpp
	vtfZImage.cpp
	vtfZImage.hpp
	vtfZSharedDevice.cpp
	vtfZSharedDevice.hpp
	vtfTextImage.cpp
	vtfTextImage.hpp
	vtfZCommandBuffer.cpp
	vtfZCommandBuffer.hpp
	vtfZDeviceMemory.hpp
	vtfProgramCollection.cpp
	vtfProgramCollection.hpp
	vtfZShaderObject.hpp
	vtfShaderObjectCollection.cpp
	vtfShaderObjectCollection.hpp
	vtfDebugMessenger.cpp
	vtfDebugMessenger.hpp
	vtfLayoutManager.cpp
	vtfLayoutManager.hpp
	vtfZPipeline.cpp
	vtfZPipeline.hpp
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
	vtfZSpecializationInfo.cpp
	vtfZSpecializationInfo.hpp
	vtfZPushConstants.cpp
	vtfZPushConstants.hpp
	vtfCogwheelTools.cpp
	vtfCogwheelTools.hpp
)

set(framework_libs pdriver driver)

if(WIN32)
list(APPEND framework_libs dbghelp)
endif()

add_library(framework STATIC ${framework_files})
target_include_directories(framework BEFORE PRIVATE ${GLFW3_INCLUDE_DIR})
target_include_directories(framework BEFORE PRIVATE ${VULKAN_INCLUDE_DIR})
target_include_directories(framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework ${framework_libs})
add_definitions(-DVULKAN_DRIVER="${VULKAN_DRIVER}")

