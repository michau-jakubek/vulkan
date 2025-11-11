
set(compiler_files
	vtfOfflineCompiler.cpp
	vtfOfflineCompiler.hpp)

if(OFFLINE_SHADER_COMPILER)
	set(compiler_files
		vtfOfflineCompilerX.cpp
		vtfOfflineCompiler.hpp)
	find_package(glslang CONFIG )
	set(compiler_libs glslang::glslang)
	# SPIRV SPIRV-Tools-opt OGLCompiler OSDependent)
else()
	set(compiler_libs)
endif()
add_library(compiler STATIC ${compiler_files})
target_link_libraries(compiler ${compiler_libs})
target_include_directories(compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../framework)
