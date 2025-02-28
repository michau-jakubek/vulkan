set(external_repos stb stl_reader glm)

#message("SOURCE ${CMAKE_SOURCE_DIR}")
#message("CURRENT_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(external ALL)

add_custom_command(TARGET external PRE_BUILD
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/external
				COMMAND ${Python_EXECUTABLE} "fetch_externals.py" ${external_repos})
