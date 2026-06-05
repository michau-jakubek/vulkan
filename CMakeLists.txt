# vtf-app: the VTF test executable. Depends on the vtf-libs package.
#
# vtf-libs is located via VTF_LIBS_DIR:
#   * set       -> use the libraries at that path;
#   * not set   -> fetch them into external/vtf-libs (download URL is a placeholder
#                  for now; the local sibling repo simulates the eventual download).

cmake_minimum_required(VERSION 3.10.2)

project(vulkan-trivial-framework CXX)

# Placeholder "remote" for vtf-libs. Defaults to the sibling repo so the split can
# be exercised locally before a real download URL exists.
set(VTF_LIBS_GIT_URL "${CMAKE_CURRENT_SOURCE_DIR}/../vtf-libs"
	CACHE STRING "Location/URL of the vtf-libs repository (used only when VTF_LIBS_DIR is not set)")

if(VTF_LIBS_DIR)
	message(STATUS "VTF_LIBS_DIR is set: using existing vtf-libs at '${VTF_LIBS_DIR}'")
else()
	set(VTF_LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vtf-libs)
	message(STATUS "VTF_LIBS_DIR is NOT set:")
	message(STATUS "  * vtf-libs will be obtained under '${VTF_LIBS_DIR}'")
	message(STATUS "  * download source (VTF_LIBS_GIT_URL) = '${VTF_LIBS_GIT_URL}'")
	message(STATUS "  * to use an existing checkout instead: -DVTF_LIBS_DIR=<path>")
	message(STATUS "  * to change the download source:       -DVTF_LIBS_GIT_URL=<url>")
	if(NOT EXISTS ${VTF_LIBS_DIR}/CMakeLists.txt)
		message(STATUS "  -> fetching vtf-libs from '${VTF_LIBS_GIT_URL}' ...")
		find_package(Git REQUIRED)
		file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} clone ${VTF_LIBS_GIT_URL} ${VTF_LIBS_DIR}
			RESULT_VARIABLE VTF_LIBS_CLONE_RESULT)
		if(NOT VTF_LIBS_CLONE_RESULT EQUAL 0)
			message(FATAL_ERROR
				"Failed to fetch vtf-libs into ${VTF_LIBS_DIR}.\n"
				"Provide -DVTF_LIBS_DIR=<path> or a valid -DVTF_LIBS_GIT_URL=<url>.")
		endif()
	else()
		message(STATUS "  -> vtf-libs already present at '${VTF_LIBS_DIR}'; skipping fetch")
	endif()
endif()

get_filename_component(VTF_LIBS_DIR "${VTF_LIBS_DIR}" ABSOLUTE)
if(NOT EXISTS ${VTF_LIBS_DIR}/CMakeLists.txt)
	message(FATAL_ERROR "VTF_LIBS_DIR=${VTF_LIBS_DIR} does not contain a vtf-libs CMakeLists.txt")
endif()
message(STATUS "Using vtf-libs from ${VTF_LIBS_DIR}")

# Shared toolchain / dependency setup (ships with vtf-libs).
include(${VTF_LIBS_DIR}/cmake/VtfCommon.cmake)

# Build the libraries as a subproject; pull in the include paths they expose.
set(VTF_LIBS_AS_SUBPROJECT ON)
add_subdirectory(${VTF_LIBS_DIR} ${CMAKE_BINARY_DIR}/vtf-libs)
include_directories(${VTF_LIBS_INCLUDE_DIRS})

set(ASSETS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets)

include_directories(tests)
add_subdirectory(tests)
add_dependencies(tests external)

include_directories(app)
add_subdirectory(app)

if(VTF_AS_DLL)
	include_directories(vtf-launcher)
	add_subdirectory(vtf-launcher)
endif()
