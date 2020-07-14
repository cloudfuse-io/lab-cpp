locals {
  lambda_build_dir = "../bin/build-amznlinux1/executables"
}

module "mem-bandwidth-lambda" {
  source = "./lambda"

  function_base_name = "mem-bandwidth-static"
  filename           = "${local.lambda_build_dir}/buzz-mem-bandwidth-static.zip"
  handler            = "N/A"
  memory_size        = 2048
  timeout            = 10
  runtime            = "provided"

  additional_policies = []
  environment = {
    MOCK = 0
  }
}

module "query-bandwidth2-lambda" {
  source = "./lambda"

  function_base_name = "query-bandwidth2-static"
  filename           = "${local.lambda_build_dir}/buzz-query-bandwidth2-static.zip"
  handler            = "N/A"
  memory_size        = 2048
  timeout            = 10
  runtime            = "provided"

  additional_policies = [aws_iam_policy.s3-additional-policy.arn]
  environment = {
    NB_CHUNCK = 12
    MAX_PARALLEL = 12
    CHUNK_SIZE  = 250000
  }
}

########### SCHEDULERS #############

module "mem-bandwidth-scheduler" {
  source = "./scheduler"

  function_name       = module.mem-bandwidth-lambda.lambda_name
  function_arn        = module.mem-bandwidth-lambda.lambda_arn
  schedule_expression = "cron(0 * * * ? *)"
}

module "query-bandwidth2-scheduler" {
  source = "./scheduler"

  function_name       = module.query-bandwidth2-lambda.lambda_name
  function_arn        = module.query-bandwidth2-lambda.lambda_arn
  schedule_expression = "cron(10 * * * ? *)"
}
