###############################################################################
# Coverage
#
# Requires latest gcovr (for GCC 8.1 support):#
# pip install git+https://github.com/gcovr/gcovr.git
###############################################################################
if(ENABLE_COVERAGE)
    find_program(GCOVR_BINARY gcovr)
    find_package_handle_standard_args(GCOVR DEFAULT_MSG GCOVR_BINARY)

    if(GCOVR_FOUND)
        get_filename_component(ABSOLUTE_LAMMPS_SOURCE_DIR ${LAMMPS_SOURCE_DIR} ABSOLUTE)

        add_custom_target(
            gen_coverage_xml
            COMMAND ${GCOVR_BINARY} -s -x -r ${ABSOLUTE_LAMMPS_SOURCE_DIR} --object-directory=${CMAKE_BINARY_DIR} -o coverage.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating XML Coverage Report..."
        )

        set(COVERAGE_HTML_DIR ${CMAKE_BINARY_DIR}/coverage_html)

        add_custom_target(coverage_html_folder
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_HTML_DIR})

        add_custom_target(
            gen_coverage_html
            COMMAND ${GCOVR_BINARY} -s  --html --html-details -r ${ABSOLUTE_LAMMPS_SOURCE_DIR} --object-directory=${CMAKE_BINARY_DIR} -o ${COVERAGE_HTML_DIR}/index.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating HTML Coverage Report..."
        )
        add_dependencies(gen_coverage_html coverage_html_folder)

        add_custom_target(clean_coverage_html
            ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_HTML_DIR}
        )

       add_custom_target(reset_coverage
            ${CMAKE_COMMAND} -E remove -f */*.gcda */*/*.gcda */*/*/*.gcda
                              */*/*/*/*.gcda */*/*/*/*/*.gcda */*/*/*/*/*/*.gcda
                              */*/*/*/*/*/*/*.gcda */*/*/*/*/*/*/*/*.gcda
                              */*/*/*/*/*/*/*/*/*.gcda */*/*/*/*/*/*/*/*/*/*.gcda
            WORKIND_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Deleting coverage report data files"
       )
    endif()
endif()
