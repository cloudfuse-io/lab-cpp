add_library (buzz-util STATIC cust_memory_pool.cc)
list(APPEND BUZZ_ALL buzz-util)
add_custom_target(cust_memory_pool-tests)
list(APPEND BUZZ_ALL cust_memory_pool-tests)

set(BUZZ_UTIL_TEST_LINK_LIBS buzz-util 
                            parquet_${BUZZ_BUILD_TYPE}
                            ${ARROW_TEST_LINK_LIBS})

add_test_case(cust_memory_pool_test
              STATIC_LINK_LIBS
              ${BUZZ_UTIL_TEST_LINK_LIBS}
              LABELS
              "cust_memory_pool-tests")

# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)