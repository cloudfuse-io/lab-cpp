resource "aws_iam_policy" "s3-additional-policy" {
  name        = "${module.env.tags["module"]}_s3_access_${module.env.region_name}_${module.env.stage}"
  description = "additional policy for s3 access"

  policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
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
