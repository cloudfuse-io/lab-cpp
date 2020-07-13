add_library (buzz-util STATIC cust_memory_pool.cc metrics.cc logger.cc scheduler.cc stats.cc)
target_include_directories(buzz-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(buzz-util)
list(APPEND BUZZ_ALL buzz-util)

## tests ##

macro(package_add_test TESTNAME)
    # create an exectuable in which the tests will be stored
    add_executable(${TESTNAME} ${ARGN})
    # link the Google test infrastructure, mocking library, and a default main fuction to
    # the test executable.  Remove g_test_main if writing your own main function.
    target_link_libraries(${TESTNAME} PRIVATE gtest gmock gtest_main)
    # TODO selectively link arrow
    target_link_arrow(${TESTNAME})
    # gtest_discover_tests replaces gtest_add_tests,
    # see https://cmake.org/cmake/help/v3.10/module/GoogleTest.html for more options to pass to it
    gtest_discover_tests(${TESTNAME}
        # set a working directory so your project root so that you can find test data via paths relative to the project root
        WORKING_DIRECTORY ${PROJECT_DIR}
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
    list(APPEND BUZZ_ALL ${TESTNAME})
endmacro()

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  package_add_test(cust_memory_pool_test cust_memory_pool_test.cc)
  package_add_test(stats_test stats_test.cc)
  package_add_test(async_queue_test async_queue_test.cc)
endif()


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)