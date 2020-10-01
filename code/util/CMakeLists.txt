add_library (cloudfuse-lab-util STATIC
  cust_memory_pool.cc
  async_queue.cc
  partial-file.cc
  metrics.cc
  logger.cc)
target_include_directories(cloudfuse-lab-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(cloudfuse-lab-util)
list(APPEND BUZZ_ALL cloudfuse-lab-util)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  package_add_test(NAME cust_memory_pool_test SRCS cust_memory_pool_test.cc DEPS cloudfuse-lab-util)
  package_add_test(NAME async_queue_test SRCS async_queue_test.cc DEPS cloudfuse-lab-util)
  package_add_test(NAME partial-file_test SRCS partial-file_test.cc DEPS cloudfuse-lab-aws cloudfuse-lab-util)
endif()


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)