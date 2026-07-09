# tests/cmake/GenerateTestsInl.cmake

function(setup_tests_inl_generation TARGET_NAME ENABLE_VTF_AS_DLL)
    set(PYTHON_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_tests_inl.py")

    set(RAW_FILES_LIST ${ARGN})
    set(CLEANED_FILES_LIST "")

    foreach(FILE_PATH ${RAW_FILES_LIST})
        get_filename_component(FILE_NAME "${FILE_PATH}" NAME)
        list(APPEND CLEANED_FILES_LIST "${FILE_NAME}")
    endforeach()

    string(REPLACE ";" " " TESTS_FILES_STR "${CLEANED_FILES_LIST}")

    add_custom_target(generate_all_tests_inl ALL
        COMMAND ${Python_EXECUTABLE} "${PYTHON_SCRIPT}" "${ENABLE_VTF_AS_DLL}" "${CMAKE_CURRENT_SOURCE_DIR}" "${TESTS_FILES_STR}"
        COMMENT "Verifying test list integrity in allTests.inl..."
        VERBATIM
    )

    add_dependencies(${TARGET_NAME} generate_all_tests_inl)
 endfunction()

