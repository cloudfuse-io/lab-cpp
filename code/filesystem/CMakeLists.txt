add_library(s3fs-forked STATIC s3fs-forked.cc aws-sdk.cc downloader.cc dispatcher.cc)
target_include_directories(s3fs-forked PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(s3fs-forked)
target_link_libraries(s3fs-forked PRIVATE buzz-util)
list(APPEND BUZZ_ALL s3fs-forked)

# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)