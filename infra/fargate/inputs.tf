module "env" {
    source = "../env"
}

variable "task_base_name" {}

variable "environment" {}

variable "vpc_id" {}

variable "task_cpu" {}

variable "task_memory" {}

variable "ecs_cluster_id" {}

variable "ecs_cluster_name" {}

variable "ecs_task_execution_role_arn" {}

variable "docker_image" {}

variable "subnets" {}

variable "local_ip" {}
