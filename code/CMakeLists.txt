project(buzz-cpp)
cmake_minimum_required(VERSION 3.11)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.8.0
  )
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
  endif()
  enable_testing()
  include(GoogleTest)
endif()

# list all the targets created here to promote them to CXX17
set(BUZZ_ALL)

# need to manually set includes because they are not exposed by arrow_${BUZZ_BUILD_TYPE} target
function(TARGET_LINK_ARROW TARGET_NAME)
  get_property(ARROW_PRIVATE_INCLUDES TARGET arrow_${BUZZ_BUILD_TYPE} PROPERTY INCLUDE_DIRECTORIES)
  get_property(PARQUET_PRIVATE_INCLUDES TARGET parquet_${BUZZ_BUILD_TYPE} PROPERTY INCLUDE_DIRECTORIES)
  target_include_directories(${TARGET_NAME} PRIVATE ${ARROW_PRIVATE_INCLUDES} ${PARQUET_PRIVATE_INCLUDES})
  target_link_libraries(${TARGET_NAME} PRIVATE arrow_${BUZZ_BUILD_TYPE} parquet_${BUZZ_BUILD_TYPE})
endfunction()

message(STATUS "AWSSDK_LINK_LIBRARIES: ${AWSSDK_LINK_LIBRARIES}")
add_subdirectory(arrow/cpp)
add_subdirectory(util)
add_subdirectory(filesystem)

# we build exec files 1 by 1, acording to the BUZZ_BUILD_FILE var
if(BUZZ_BUILD_FILE)
  find_package(aws-lambda-runtime)
  set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

  MESSAGE(STATUS "Packaging ${BUZZ_BUILD_FILE}.cc as ${BUZZ_TARGET}")

  add_executable(${BUZZ_TARGET} playground/${BUZZ_BUILD_FILE}.cc)
  list(APPEND BUZZ_ALL ${BUZZ_TARGET})

  target_link_libraries(${BUZZ_TARGET}
    PRIVATE AWS::aws-lambda-runtime s3fs-forked buzz-util
  )
  target_link_arrow(${BUZZ_TARGET})
  aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)
else()
  MESSAGE(STATUS "Not packaging any executable")
endif()

set_property(TARGET ${BUZZ_ALL} PROPERTY CXX_STANDARD 17)

