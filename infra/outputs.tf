output "subnet_ids" {
  value = module.vpc.public_subnets
}

output "query-bw-scheduler-fargate_security_group_id" {
  value = module.query-bw-scheduler-fargate.security_group_id
}

data "aws_caller_identity" "current" {}

output "account_id" {
  value = "${data.aws_caller_identity.current.account_id}"
}
