module "env" {
  source = "./env"
}

module "parquet-arrow-reader-static" {
  source = "./lambda"

  function_name = "parquet-arrow-reader-static"
  filename      = "../bin/build-amznlinux1/buzz-parquet-arrow-reader-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MAX_CONCURRENT_DL : 8
    MAX_CONCURRENT_PROC : 1
    COLUMN_ID : 16
    AS_DICT : "true"
  }
}

module "parquet-raw-reader-static" {
  source = "./lambda"

  function_name = "parquet-raw-reader-static"
  filename      = "../bin/build-amznlinux1/buzz-parquet-raw-reader-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MAX_CONCURRENT_DL : 8
    MAX_CONCURRENT_PROC : 1
    COLUMN_ID : 16
  }
}

module "query-bandwidth-static" {
  source = "./lambda"

  function_name = "query-bandwidth-static"
  filename      = "../bin/build-amznlinux1/buzz-query-bandwidth-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}

module "mem-alloc-overprov-static" {
  source = "./lambda"

  function_name = "mem-alloc-overprov-static"
  filename      = "../bin/build-amznlinux1/buzz-mem-alloc-overprov-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    NB_ALLOCATION : 100
    ALLOCATION_SIZE_BYTE : 1048576
    NB_REPETITION : 100
  }
}

module "mem-alloc-speed-static" {
  source = "./lambda"

  function_name = "mem-alloc-speed-static"
  filename      = "../bin/build-amznlinux1/buzz-mem-alloc-speed-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    NB_ALLOCATION : 100
    ALLOCATION_SIZE_BYTE : 1048576
  }
}

module "simd-support-static" {
  source = "./lambda"

  function_name = "simd-support-static"
  filename      = "../bin/build-amznlinux1/buzz-simd-support-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = 0
  }
}

module "raw-alloc-static" {
  source = "./lambda"

  function_name = "raw-alloc-static"
  filename      = "../bin/build-amznlinux1/buzz-raw-alloc-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    NB_PAGE         = 1024
    ALLOC_TEST_NAME = "mmap_madv_newplace"
  }
}
