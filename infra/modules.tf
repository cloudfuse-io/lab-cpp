module "env" {
  source = "./env"
}

module "parquet-reader-static" {
  source = "./lambda"

  function_name = "parquet-reader-static"
  filename      = "../bin/build-amznlinux1/buzz/buzz-parquet-reader-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MAX_CONCURRENT_DL: 8
    MAX_CONCURRENT_PROC: 1
    COLUMN_NAME: "href"
  }
}

module "query-bandwidth-static" {
  source = "./lambda"

  function_name = "query-bandwidth-static"
  filename      = "../bin/build-amznlinux1/buzz/buzz-query-bandwidth-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}

module "mem-alloc-static" {
  source = "./lambda"

  function_name = "mem-alloc-static"
  filename      = "../bin/build-amznlinux1/buzz/buzz-mem-alloc-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MEGA_ALLOCATED = 1000
  }
}

module "simd-support-static" {
  source = "./lambda"

  function_name = "simd-support-static"
  filename      = "../bin/build-amznlinux1/buzz/buzz-simd-support-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}