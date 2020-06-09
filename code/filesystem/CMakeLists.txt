add_library(s3fs-forked STATIC s3fs-forked.cc)
target_include_directories(buzz-util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(s3fs-forked PRIVATE buzz-util)
list(APPEND BUZZ_ALL s3fs-forked)

# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)