add_library (buzz-util STATIC cust_memory_pool.cc metrics.cc scheduler.cc stats.cc)
target_include_directories(buzz-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND BUZZ_ALL buzz-util)

# tests
set(BUZZ_UTIL_TEST_LINK_LIBS buzz-util 
                            parquet_${BUZZ_BUILD_TYPE}
                            ${ARROW_TEST_LINK_LIBS})

function(ADD_BUZZ_TEST_CASE TEST_FILE)
  set(TEST_NAME "${TEST_FILE}-target")
  add_executable(${TEST_NAME} "${TEST_FILE}.cc")
  target_link_libraries(${TEST_NAME} PRIVATE ${BUZZ_UTIL_TEST_LINK_LIBS})
  # it is not enough to set CXX version at the end
  set_property(TARGET ${TEST_NAME} PROPERTY CXX_STANDARD 17)
  add_test(${TEST_NAME}
          ${BUILD_SUPPORT_DIR}/run-test.sh
          ${CMAKE_BINARY_DIR}
          test
          "${EXECUTABLE_OUTPUT_PATH}/${TEST_NAME}")
endfunction()

add_buzz_test_case(cust_memory_pool_test)
add_buzz_test_case(stats_test)


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)