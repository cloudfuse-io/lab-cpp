add_library(cloudfuse-lab-aws STATIC
  sdk-init.cc
  downloader.cc
  curl/HttpClientFactory.cpp
  curl/HttpClient.cpp
  curl/HandleContainer.cpp)
target_include_directories(cloudfuse-lab-aws PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_arrow(cloudfuse-lab-aws)
target_link_libraries(cloudfuse-lab-aws PRIVATE cloudfuse-lab-util)

find_package(AWSSDK COMPONENTS config s3 transfer lambda)
target_link_libraries(cloudfuse-lab-aws PRIVATE ${AWSSDK_LINK_LIBRARIES})

list(APPEND BUZZ_ALL cloudfuse-lab-aws)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  package_add_test(NAME downloader_test SRCS downloader_test.cc DEPS cloudfuse-lab-aws cloudfuse-lab-util)
endif()


# this is required to propagate the added targets to the parent scope
set(BUZZ_ALL "${BUZZ_ALL}" PARENT_SCOPE)