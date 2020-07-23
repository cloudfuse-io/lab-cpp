cmake_minimum_required(VERSION 3.17)
project(buzz-cpp VERSION 0.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/executables)

include(FetchContent)

if("${BUZZ_BUILD_TESTS}" STREQUAL "ON")
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.8.0
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  include(GoogleTest)
  # custom test utils:
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
  include(TestUtils)
endif()



# list all the targets created here to promote them to CXX17
set(BUZZ_ALL)

# need to manually set includes because they are not exposed by arrow_${BUZZ_BUILD_TYPE} target
function(TARGET_LINK_ARROW TARGET_NAME)
  get_property(ARROW_PRIVATE_INCLUDES TARGET arrow_${BUZZ_BUILD_TYPE} PROPERTY INCLUDE_DIRECTORIES)
  get_property(PARQUET_PRIVATE_INCLUDES TARGET parquet_${BUZZ_BUILD_TYPE} PROPERTY INCLUDE_DIRECTORIES)
  target_include_directories(${TARGET_NAME} PRIVATE ${ARROW_PRIVATE_INCLUDES} ${PARQUET_PRIVATE_INCLUDES})
  target_link_libraries(${TARGET_NAME} PRIVATE arrow_${BUZZ_BUILD_TYPE} parquet_${BUZZ_BUILD_TYPE} arrow_flight_${BUZZ_BUILD_TYPE})
  # help flight find openssl
  find_package(OpenSSL)
  target_link_libraries(arrow_flight_${BUZZ_BUILD_TYPE}
    PRIVATE OpenSSL::Crypto OpenSSL::SSL
  )
endfunction()

set(CMAKE_BUILD_TYPE_SAVED "${CMAKE_BUILD_TYPE}")
set(CMAKE_BUILD_TYPE Release)
add_subdirectory(arrow/cpp)
set(CMAKE_BUILD_TYPE "${BUILD_TYPE_SAVED}")
add_subdirectory(util)
add_subdirectory(aws)

# TODO remove following when s3fs fully replaced
find_package(AWSSDK COMPONENTS config s3 transfer lambda)
message(STATUS "Found AWS SDK libraries: ${AWSSDK_LINK_LIBRARIES}")
target_link_libraries(arrow_${BUZZ_BUILD_TYPE} PRIVATE ${AWSSDK_LINK_LIBRARIES})

# we build exec files 1 by 1, acording to the BUZZ_BUILD_FILE var
set(HIVE_FILES flight-server query-bw-scheduler)
if("${BUZZ_BUILD_FILE}" IN_LIST HIVE_FILES)
  set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

  MESSAGE(STATUS "Hive packaging ${BUZZ_BUILD_FILE}.cc as ${BUZZ_TARGET}")

  add_executable(${BUZZ_TARGET} playground/${BUZZ_BUILD_FILE}.cc)
  list(APPEND BUZZ_ALL ${BUZZ_TARGET})

  target_link_libraries(${BUZZ_TARGET}
    PRIVATE buzz-aws buzz-util
  )
  target_link_arrow(${BUZZ_TARGET})
elseif(BUZZ_BUILD_FILE)
  find_package(aws-lambda-runtime)
  set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

  MESSAGE(STATUS "Bee packaging ${BUZZ_BUILD_FILE}.cc as ${BUZZ_TARGET}")

  add_executable(${BUZZ_TARGET} playground/${BUZZ_BUILD_FILE}.cc)
  list(APPEND BUZZ_ALL ${BUZZ_TARGET})

  target_link_libraries(${BUZZ_TARGET}
    PRIVATE AWS::aws-lambda-runtime buzz-aws buzz-util
  )
  target_link_arrow(${BUZZ_TARGET})
else()
  MESSAGE(STATUS "Not packaging any executable")
endif()

set_property(TARGET ${BUZZ_ALL} PROPERTY CXX_STANDARD 17)

