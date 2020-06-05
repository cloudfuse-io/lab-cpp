add_library (buzz-util STATIC cust_memory_pool.cc)
add_custom_target(cust_memory_pool-tests)

set(BUZZ_UTIL_TEST_LINK_LIBS buzz-util 
                            parquet_${BUZZ_BUILD_TYPE}
                            ${ARROW_TEST_LINK_LIBS})

add_test_case(cust_memory_pool_test
              STATIC_LINK_LIBS
              ${BUZZ_UTIL_TEST_LINK_LIBS}
              LABELS
              "cust_memory_pool-tests")