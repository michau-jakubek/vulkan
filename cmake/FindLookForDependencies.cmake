
function(find_headers_and_libs in_hdr in_win_lib in_lnx_lib in_look_dir in_hint_dir_comma_list out_inc_dir out_lib_path)
	#message("Python_EXECUTABLE: ${Python_EXECUTABLE}")
	set(in_lib ${in_lnx_lib})
	set(ignore_case 0)
	if (WIN32)
		set(in_lib ${in_win_lib})
		set(ignore_case 1)
	endif()
	execute_process(
		COMMAND ${Python_EXECUTABLE} "find_headers_and_libs.py" ${in_hdr} ${in_lib} ${in_look_dir} "${in_hint_dir_comma_list}" ${ignore_case}
		WORKING_DIRECTORY ${PROJECT_MODULES_PATH}
		OUTPUT_VARIABLE hdr_and_lib)
	#message("find_header_and_libs.py returned(hdr_and_lib):" ${hdr_and_lib})
	list(POP_FRONT hdr_and_lib hdr lib)
	#message("find_header_and_libs.py returned(hdr):" ${hdr})
	#message("find_header_and_libs.py returned(lib):" ${lib})
	set(${out_inc_dir} ${hdr} PARENT_SCOPE)
	set(${out_lib_path} ${lib} PARENT_SCOPE)
endfunction()

function(find_vulkan in_look_dir in_diff_dir out_inc_dir out_lib_dir)
	#message(STATUS "Looking for Vulkan dependencies, in_look_dir: ${in_look_dir}, in_diff_dir: ${in_diff_dir}")
	find_headers_and_libs("vulkan_core.h" "vulkan-1.lib" "libvulkan.so" "${in_look_dir}" "${in_diff_dir}" out_inc_path out_lib_path)
	set(${out_inc_dir} ${out_inc_path} PARENT_SCOPE)
	set(${out_lib_dir} ${out_lib_path} PARENT_SCOPE)
endfunction()

function(find_glfw3 in_look_dir in_diff_dir out_inc_dir out_lib_dir)
	#message(STATUS "Looking for GLFW3 dependencies, in_look_dir: ${in_look_dir}, in_diff_dir: ${in_diff_dir}")
	find_headers_and_libs("glfw3.h" "glfw3.lib" "libglfw3.a" "${in_look_dir}" "${in_diff_dir}" out_inc_path out_lib_path)
	set(${out_inc_dir} ${out_inc_path} PARENT_SCOPE)
	set(${out_lib_dir} ${out_lib_path} PARENT_SCOPE)
endfunction()

