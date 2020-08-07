resource "aws_ecr_repository" "query-bw-scheduler-repo" {
  name                 = "${module.env.tags["module"]}-query-bw-scheduler-${module.env.stage}"
  image_tag_mutability = "MUTABLE"

  image_scanning_configuration {
    scan_on_push = false
  }
}

resource "null_resource" "query-bw-scheduler-push" {
  triggers = {
    always_run = "${timestamp()}"
  }

  provisioner "local-exec" {
    command = <<EOT
      docker tag "buzz-hive-query-bw-scheduler:${var.git_revision}" "${aws_ecr_repository.query-bw-scheduler-repo.repository_url}:${var.git_revision}"
      docker push "${aws_ecr_repository.query-bw-scheduler-repo.repository_url}:${var.git_revision}"
    EOT
  }
}

module "query-bw-scheduler-fargate" {
  source = "./fargate"

  task_base_name              = "query-bw-scheduler-static"
  vpc_id                      = module.vpc.vpc_id
  task_cpu                    = 2048
  task_memory                 = 4096
  ecs_cluster_id              = aws_ecs_cluster.ecs_cluster.id
  ecs_cluster_name            = aws_ecs_cluster.ecs_cluster.name
  ecs_task_execution_role_arn = aws_iam_role.ecs_task_execution_role.arn
  docker_image                = "${aws_ecr_repository.query-bw-scheduler-repo.repository_url}:${var.git_revision}"
  subnets                     = module.vpc.public_subnets
  environment = [{
    name  = "BEE_FUNCTION_NAME",
    value = module.query-bandwidth-bee.lambda_name
    }, {
    name  = "NB_PARALLEL",
    value = "64"
    }, {
    name  = "NB_INVOKE",
    value = "512"
  }]
  local_ip              = "${chomp(data.http.icanhazip.body)}/32"
  depends_on_image_push = null_resource.query-bw-scheduler-push.id
}

module "query-bandwidth-bee" {
  source = "./lambda"

  function_base_name = "query-bandwidth-static-bee"
  filename           = "${local.lambda_build_dir}/buzz-query-bandwidth-static.zip"
  handler            = "N/A"
  memory_size        = 2048
  timeout            = 10
  runtime            = "provided"

  additional_policies = [aws_iam_policy.s3-additional-policy.arn]
  environment = {
    NB_CHUNCK    = 12
    MAX_PARALLEL = 12
    CHUNK_SIZE   = 16000000
  }
}
