add_library (buzz-bee STATIC
  dispatcher.cc
  stats.cc)
target_include_directories(buzz-bee PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(buzz-bee)
target_link_libraries(buzz-bee PRIVATE buzz-util)
list(APPEND BUZZ_ALL buzz-bee)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  package_add_test(NAME stats_test SRCS stats_test.cc DEPS buzz-util)
endif()


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)