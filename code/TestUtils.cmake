# this function builds the SRCS test as NAME with arrow and the DEPS libraries linked
function(package_add_test)
    cmake_parse_arguments(
        PARSED_ARGS # prefix of output variables
        "" # list of names of the boolean arguments (only defined ones will be true)
        "NAME" # list of names of mono-valued arguments
        "SRCS;DEPS" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )
    # create an exectuable in which the tests will be stored
    add_executable(${PARSED_ARGS_NAME} ${PARSED_ARGS_SRCS})
    # link the Google test infrastructure, mocking library, and a default main fuction to
    # the test executable.  Remove g_test_main if writing your own main function.
    target_link_libraries(${PARSED_ARGS_NAME} PRIVATE gtest gmock gtest_main)
    # TODO selectively link arrow
    target_link_arrow(${PARSED_ARGS_NAME})
    target_link_libraries(${PARSED_ARGS_NAME} PRIVATE ${PARSED_ARGS_DEPS})
    # gtest_discover_tests replaces gtest_add_tests,
    # see https://cmake.org/cmake/help/v3.10/module/GoogleTest.html for more options to pass to it
    gtest_discover_tests(${PARSED_ARGS_NAME}
        # set a working directory so your project root so that you can find test data via paths relative to the project root
        WORKING_DIRECTORY ${PROJECT_DIR}
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${PARSED_ARGS_NAME} PROPERTIES FOLDER tests)
    list(APPEND BUZZ_ALL ${PARSED_ARGS_NAME})
    set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)
endfunction()