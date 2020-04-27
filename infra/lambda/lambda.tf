resource "aws_lambda_function" "lambda" {
  filename         = var.filename
  function_name    = "${module.env.tags["module"]}-${var.function_name}-${module.env.stage}"
  role             = aws_iam_role.lambda_role.arn
  handler          = var.handler
  memory_size      = var.memory_size
  timeout          = var.timeout
  source_code_hash = filebase64sha256(var.filename)
  runtime          = var.runtime

  environment {
    variables = merge(
      {
        STAGE = module.env.stage
      },
      var.environment
    )
  }

  tags = module.env.tags
}

resource "aws_lambda_function_event_invoke_config" "lambda_conf" {
  function_name                = aws_lambda_function.lambda.function_name
  maximum_event_age_in_seconds = 60
  maximum_retry_attempts       = 0
}


resource "aws_cloudwatch_log_group" "lambda_log_group" {
  name              = "/aws/lambda/${aws_lambda_function.lambda.function_name}"
  retention_in_days = 14
  tags              = module.env.tags
}
