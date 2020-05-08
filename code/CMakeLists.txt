find_package(aws-lambda-runtime)

set(BUZZ_TARGET "buzz-${BUZZ_BUILD_FILE}-${BUZZ_BUILD_TYPE}")

add_executable(${BUZZ_TARGET} tests/${BUZZ_BUILD_FILE}.cc)

target_link_libraries(${BUZZ_TARGET}
  PRIVATE parquet_${BUZZ_BUILD_TYPE} AWS::aws-lambda-runtime
)

add_dependencies(parquet
  ${BUZZ_TARGET}
)

aws_lambda_package_target(${BUZZ_TARGET} NO_LIBC)