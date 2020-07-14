locals {
  env = [
    {
      name  = "AWS_REGION"
      value = module.env.region_name
    }
  ]
  container_definition = [
    {
      cpu         = var.task_cpu
      image       = var.docker_image
      memory      = var.task_memory
      name        = "${var.task_base_name}"
      essential   = true
      mountPoints = []
      portMappings = [
        {
          containerPort = 8080
          hostPort      = 8080
          protocol      = "tcp"
        },
        {
          containerPort = 3333
          hostPort      = 3333
          protocol      = "tcp"
        },
      ]
      volumesFrom = []
      environment = concat(var.environment, local.env)
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          awslogs-group         = aws_cloudwatch_log_group.fargate_logging.name
          awslogs-region        = module.env.region_name
          awslogs-stream-prefix = "ecs"
        }
      }
    },
  ]
}