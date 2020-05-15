module "env" {
  source = "./env"
}

module "parquet-reader-static" {
  source = "./lambda"

  function_name = "parquet-reader-static"
  filename      = "../bin/build/buzz/buzz-parquet-reader-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}

module "query-bandwidth-static" {
  source = "./lambda"

  function_name = "query-bandwidth-static"
  filename      = "../bin/build/buzz/buzz-query-bandwidth-static.zip"
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
  filename      = "../bin/build/buzz/buzz-mem-alloc-static.zip"
  handler       = "N/A"
  memory_size   = 2048
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}

module "simd-support-static" {
  source = "./lambda"

  function_name = "simd-support-static"
  filename      = "../bin/build/buzz/buzz-simd-support-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}