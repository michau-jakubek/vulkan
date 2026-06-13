# tests/cmake/GenerateTestsInl.cmake

function(setup_tests_inl_generation TARGET_NAME ENABLE_VTF_AS_DLL TESTS_FILES_LIST)
    set(PYTHON_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_tests_inl.py")

    add_custom_target(generate_all_tests_inl ALL
        COMMAND ${Python_EXECUTABLE} ${PYTHON_SCRIPT} ${ENABLE_VTF_AS_DLL} ${CMAKE_CURRENT_SOURCE_DIR} ${TESTS_FILES_LIST}
        COMMENT "Verifying test list integrity in w allTests.inl..."
        VERBATIM
    )

    add_dependencies(${TARGET_NAME} generate_all_tests_inl)
endfunction()

