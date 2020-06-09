find_package(aws-lambda-runtime)

# list all the targets created here to promote them to CXX17
set(BUZZ_ALL)

add_subdirectory(util)
add_subdirectory(filesystem)

# we build exec files 1 by 1, acording to the BUZZ_BUILD_FILE var
if(BUZZ_BUILD_FILE)
  set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

  MESSAGE(STATUS "Packaging ${BUZZ_BUILD_FILE}.cc as ${BUZZ_TARGET}")

  add_executable(${BUZZ_TARGET} tests/${BUZZ_BUILD_FILE}.cc)
  list(APPEND BUZZ_ALL ${BUZZ_TARGET})

  target_link_libraries(${BUZZ_TARGET}
    PRIVATE parquet_${BUZZ_BUILD_TYPE} AWS::aws-lambda-runtime s3fs-forked buzz-util
  )
  # target_include_directories(${BUZZ_TARGET} PUBLIC util)

  add_dependencies(parquet
    ${BUZZ_TARGET}
  )

  aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)
else()
  MESSAGE(STATUS "Not packaging any executable")
endif()

set_property(TARGET ${BUZZ_ALL} PROPERTY CXX_STANDARD 17)

