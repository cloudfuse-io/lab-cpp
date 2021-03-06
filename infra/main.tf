terraform {
  backend "s3" {}
  required_version = ">=0.12"
}

variable "profile" {}

variable "git_revision" {
  default = "unknown"
}

provider "aws" {
  profile = var.profile
  version = "2.70.0"
  region  = module.env.region_name
}

module "env" {
  source = "./env"
}

provider "http" {
  version = "~> 1.2"
}

data "http" "icanhazip" {
  url = "http://icanhazip.com"
}

provider "null" {
  version = "~> 2.1"
}
