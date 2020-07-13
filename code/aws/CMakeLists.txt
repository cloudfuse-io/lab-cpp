add_library(buzz-aws STATIC
  s3fs-forked.cc
  sdk-init.cc
  downloader.cc
  curl/HttpClientFactory.cpp
  curl/HttpClient.cpp
  curl/HandleContainer.cpp)
target_include_directories(buzz-aws PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(buzz-aws)
target_link_libraries(buzz-aws PRIVATE buzz-util)
list(APPEND BUZZ_ALL buzz-aws)

# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)