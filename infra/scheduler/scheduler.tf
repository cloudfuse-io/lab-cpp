resource "aws_cloudwatch_event_rule" "scheduler_trigger_rule" {
  name        = var.function_name
  description = "Trigger scheduler lambda to collect its performance metrics"

  schedule_expression = var.schedule_expression
}

resource "aws_cloudwatch_event_target" "scheduler_trigger_target" {
  rule      = aws_cloudwatch_event_rule.scheduler_trigger_rule.name
  target_id = var.function_name
  arn       = var.function_arn
}

resource "aws_lambda_permission" "allow_cloudwatch_trigger_events" {
  statement_id  = var.function_name
  action        = "lambda:InvokeFunction"
  function_name = var.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.scheduler_trigger_rule.arn
}