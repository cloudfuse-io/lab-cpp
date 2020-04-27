resource "aws_iam_policy" "scanner-additional-policy" {
  name        = "${module.env.tags["module"]}_scanner_custom_${module.env.stage}"
  description = "additional policy for lambda poc scanner"

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Action": [
        "sqs:*"
      ],
      "Resource": "*",
      "Effect": "Allow"
    },
    {
      "Action": [
        "s3:*"
      ],
      "Resource": "*",
      "Effect": "Allow"
    }
  ]
}
EOF
}
