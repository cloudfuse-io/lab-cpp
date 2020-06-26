module "parquet-arrow-reader-lambda" {
  source = "./lambda"

  function_base_name = "parquet-arrow-reader-static"
  filename      = "../bin/build-amznlinux1/buzz-parquet-arrow-reader-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.s3-additional-policy.arn]
  environment = {
    MAX_CONCURRENT_DL : 8
    MAX_CONCURRENT_PROC : 1
    COLUMN_ID : 16
    AS_DICT : "true"
  }
}

module "parquet-raw-reader-lambda" {
  source = "./lambda"

  function_base_name = "parquet-raw-reader-static"
  filename      = "../bin/build-amznlinux1/buzz-parquet-raw-reader-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.s3-additional-policy.arn]
  environment = {
    MAX_CONCURRENT_DL : 8
    MAX_CONCURRENT_PROC : 1
    COLUMN_ID : 16
  }
}

module "query-bandwidth-lambda" {
  source = "./lambda"

  function_base_name = "query-bandwidth-static"
  filename      = "../bin/build-amznlinux1/buzz-query-bandwidth-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.s3-additional-policy.arn]
  environment = {
    MOCK = "mock"
    NB_PARALLEL = 12
    CHUNK_SIZE = 250000
  }
}

module "mem-alloc-overprov-lambda" {
  source = "./lambda"

  function_base_name = "mem-alloc-overprov-static"
  filename      = "../bin/build-amznlinux1/buzz-mem-alloc-overprov-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    NB_ALLOCATION : 100
    ALLOCATION_SIZE_BYTE : 1048576
    NB_REPETITION : 100
  }
}

module "mem-alloc-speed-lambda" {
  source = "./lambda"

  function_base_name = "mem-alloc-speed-static"
  filename      = "../bin/build-amznlinux1/buzz-mem-alloc-speed-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    NB_ALLOCATION : 100
    ALLOCATION_SIZE_BYTE : 1048576
  }
}

module "simd-support-lambda" {
  source = "./lambda"

  function_base_name = "simd-support-static"
  filename      = "../bin/build-amznlinux1/buzz-simd-support-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    MOCK = 0
  }
}

module "mem-bandwidth-lambda" {
  source = "./lambda"

  function_base_name = "mem-bandwidth-static"
  filename      = "../bin/build-amznlinux1/buzz-mem-bandwidth-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    MOCK = 0
  }
}

module "raw-alloc-lambda" {
  source = "./lambda"

  function_base_name = "raw-alloc-static"
  filename      = "../bin/build-amznlinux1/buzz-raw-alloc-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    NB_PAGE         = 1024
    ALLOC_TEST_NAME = "mmap_madv_newplace"
  }
}

module "core-affinity-lambda" {
  source = "./lambda"

  function_base_name = "core-affinity-static"
  filename      = "../bin/build-amznlinux1/buzz-core-affinity-static.zip"
  handler       = "N/A"
  memory_size   = 1792
  timeout       = 10
  runtime       = "provided"

  additional_policies = []
  environment = {
    CPU_FOR_SECOND_THREAD = 1
  }
}

########### SCHEDULERS #############

module "mem-bandwidth-scheduler" {
  source = "./scheduler"

  function_name       = module.mem-bandwidth-lambda.lambda_name
  function_arn        = module.mem-bandwidth-lambda.lambda_arn
  schedule_expression = "cron(0 * * * ? *)"
}
