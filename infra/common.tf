#### IAM ####

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

resource "aws_iam_role" "ecs_task_execution_role" {
  name = "lambda_poc_task_execution_${module.env.stage}_${module.env.region_name}"

  assume_role_policy = <<EOF
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "",
      "Effect": "Allow",
      "Principal": {
        "Service": "ecs-tasks.amazonaws.com"
      },
      "Action": "sts:AssumeRole"
    }
  ]
}
EOF

  tags = module.env.tags
}

resource "aws_iam_role_policy" "ecs_task_execution_policy" {
  name = "lambda_poc_task_execution_${module.env.stage}_${module.env.region_name}"
  role = aws_iam_role.ecs_task_execution_role.id

  policy = <<EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "ecr:GetAuthorizationToken",
                "ecr:BatchCheckLayerAvailability",
                "ecr:GetDownloadUrlForLayer",
                "ecr:BatchGetImage",
                "logs:CreateLogStream",
                "logs:PutLogEvents",
                "ecs:StartTelemetrySession"
            ],
            "Resource": "*"
        }
    ]
}
EOF
}

#### ECS ####
resource "aws_ecs_cluster" "ecs_cluster" {
    name = "${module.env.tags["module"]}-cluster-${module.env.stage}"
    capacity_providers = ["FARGATE"]
    default_capacity_provider_strategy {
        capacity_provider = "FARGATE"
    }
    setting {
        name = "containerInsights"
        value = "disabled"
    }
    tags = module.env.tags
}

#### NETWORK ####
module "vpc" {
    source = "github.com/terraform-aws-modules/terraform-aws-vpc"

    name = "${module.env.tags["module"]}-vpc-${module.env.stage}"
    cidr = module.env.vpc_cidr
    azs = module.env.vpc_azs
    public_subnets = module.env.subnet_cidrs

    enable_nat_gateway = false
    enable_vpn_gateway = false
    enable_s3_endpoint = true

    tags = module.env.tags
}
