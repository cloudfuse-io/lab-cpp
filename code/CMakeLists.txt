find_package(aws-lambda-runtime)

add_library (s3fs-forked STATIC filesystem/s3fs-forked.cc filesystem/scheduler.cc)
target_include_directories(s3fs-forked PUBLIC filesystem util)

add_subdirectory(util)

set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

if(BUZZ_BUILD_FILE)
  MESSAGE(STATUS "Packaging ${BUZZ_BUILD_FILE}")
  add_executable(${BUZZ_TARGET} tests/${BUZZ_BUILD_FILE}.cc)

  target_link_libraries(${BUZZ_TARGET}
    PRIVATE parquet_${BUZZ_BUILD_TYPE} AWS::aws-lambda-runtime s3fs-forked buzz-util
  )
  # target_include_directories(${BUZZ_TARGET} PUBLIC util)

  set_property(TARGET ${BUZZ_TARGET} s3fs-forked buzz-util PROPERTY CXX_STANDARD 17)

  add_dependencies(parquet
    ${BUZZ_TARGET}
  )

  aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)
else()
  MESSAGE(STATUS "Not packaging any executable")
endif()

