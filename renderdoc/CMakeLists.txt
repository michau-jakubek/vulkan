set(renderdoc_files vtfRenderDocMissing.cpp vtfRenderDoc.hpp)
set(renderdoc_include)

if (RENDERDOC_APP_HEADER)
	get_filename_component(renderdoc_abs_include ${RENDERDOC_APP_HEADER} ABSOLUTE)
	get_filename_component(renderdoc_include ${renderdoc_abs_include} DIRECTORY)
	set(renderdoc_files vtfRenderDoc.cpp vtfRenderDoc.hpp)
    if (${SYSTEM_OS_LINUX})
	    list(APPEND renderdoc_files lin/vtfRenderDoc.cpp)
    else()
	    list(APPEND renderdoc_files win/vtfRenderDoc.cpp)
	endif()
endif()

add_library(renderdoc STATIC ${renderdoc_files})
#target_link_libraries(renderdoc ${renderdoc_libs})
target_include_directories(renderdoc PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../framework ${renderdoc_include})
