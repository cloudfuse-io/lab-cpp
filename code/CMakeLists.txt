find_package(aws-lambda-runtime)

add_library (s3fs STATIC filesystem/s3fs.cc filesystem/s3fs.h)
target_include_directories(s3fs PUBLIC filesystem)

set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

add_executable(${BUZZ_TARGET} tests/${BUZZ_BUILD_FILE}.cc)

target_link_libraries(${BUZZ_TARGET}
  PRIVATE parquet_${BUZZ_BUILD_TYPE} AWS::aws-lambda-runtime s3fs
)

add_dependencies(parquet
  ${BUZZ_TARGET}
)

aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)