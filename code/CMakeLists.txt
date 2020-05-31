find_package(aws-lambda-runtime)

add_library (s3fs-extended STATIC filesystem/s3fs-forked.cc filesystem/scheduler.cc)
target_include_directories(s3fs-extended PUBLIC filesystem util)

set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

add_executable(${BUZZ_TARGET} tests/${BUZZ_BUILD_FILE}.cc)

target_link_libraries(${BUZZ_TARGET}
  PRIVATE parquet_${BUZZ_BUILD_TYPE} AWS::aws-lambda-runtime s3fs-extended
)
target_include_directories(${BUZZ_TARGET} PUBLIC util)

set_property(TARGET ${BUZZ_TARGET} s3fs-extended PROPERTY CXX_STANDARD 17)

add_dependencies(parquet
  ${BUZZ_TARGET}
)

aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)