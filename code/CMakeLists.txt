find_package(aws-lambda-runtime)

add_executable(buzz-test1-static tests/reader-writer-from-examples.cc)
add_executable(buzz-test1-shared tests/reader-writer-from-examples.cc)

target_link_libraries(buzz-test1-static
  PRIVATE parquet_static AWS::aws-lambda-runtime
)

target_link_libraries(buzz-test1-shared
  PRIVATE parquet_shared AWS::aws-lambda-runtime
)

add_dependencies(parquet
  buzz-test1-static
  buzz-test1-shared
)

aws_lambda_package_target(buzz-test1-static NO_LIBC)
aws_lambda_package_target(buzz-test1-shared NO_LIBC)