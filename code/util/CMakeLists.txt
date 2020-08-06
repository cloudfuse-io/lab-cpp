add_library (buzz-util STATIC
  cust_memory_pool.cc
  dispatcher.cc
  metrics.cc
  logger.cc
  stats.cc)
target_include_directories(buzz-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(buzz-util)
list(APPEND BUZZ_ALL buzz-util)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  package_add_test(NAME cust_memory_pool_test SRCS cust_memory_pool_test.cc DEPS buzz-util)
  package_add_test(NAME stats_test SRCS stats_test.cc DEPS buzz-util)
  package_add_test(NAME async_queue_test SRCS async_queue_test.cc DEPS buzz-util)
endif()


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)