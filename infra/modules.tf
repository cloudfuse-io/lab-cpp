module "env" {
  source = "./env"
}

module "scanner-cpp-static" {
  source = "./lambda"

  function_name = "test1-cpp-static"
  filename      = "../bin/build/buzz/buzz-test1-static.zip"
  handler       = "N/A"
  memory_size   = 128
  timeout       = 10
  runtime       = "provided"

  additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
  environment = {
    MOCK = "mock"
  }
}

# module "scanner-cpp-shared" {
#   source = "./lambda"

#   function_name = "test1-cpp-shared"
#   filename      = "../bin/build/buzz/buzz-test1-shared.zip"
#   handler       = "N/A"
#   memory_size   = 128
#   timeout       = 10
#   runtime       = "provided"

#   additional_policies = [aws_iam_policy.scanner-additional-policy.arn]
#   environment = {
#     MOCK = "mock"
#   }
# }
