variable generic_playground_file {}

locals {
  playground = {
    parquet-arrow-reader = {
      memory_size = 2048
      environment = {
        MAX_CONCURRENT_DL : 8
        NB_CONN_INIT : 1
        COLUMN_ID : 16
        AS_DICT : "true"
      }
      additional_policies = [aws_iam_policy.s3-additional-policy.arn]
    }
    parquet-raw-reader = {
      memory_size = 2048
      environment = {
        MAX_CONCURRENT_DL : 8
        NB_CONN_INIT : 1
        COLUMN_ID : 16
      }
      additional_policies = [aws_iam_policy.s3-additional-policy.arn]
    }
    query-bandwidth = {
      memory_size = 2048
      environment = {
        NB_CHUNCK : 12
        MAX_PARALLEL : 12
        CHUNK_SIZE : 250000
      }
      additional_policies = [aws_iam_policy.s3-additional-policy.arn]
    }
    mem-alloc-overprov = {
      memory_size = 2048
      environment = {
        NB_ALLOCATION : 100
        ALLOCATION_SIZE_BYTE : 1048576
        NB_REPETITION : 100
      }
      additional_policies = []
    }
    mem-alloc-speed = {
      memory_size = 2048
      environment = {
        NB_ALLOCATION : 100
        ALLOCATION_SIZE_BYTE : 1048576
      }
      additional_policies = []
    }
    simd-support = {
      memory_size = 128
      environment = {
        MOCK = 0
      }
      additional_policies = []
    }
    mem-bandwidth = {
      memory_size = 2048
      environment = {
        MOCK = 0
      }
      additional_policies = []
    }
    raw-alloc = {
      memory_size = 2048
      environment = {
        NB_PAGE         = 1024
        ALLOC_TEST_NAME = "mmap_madv_newplace"
      }
      additional_policies = []
    }
    core_affinity = {
      memory_size = 2048
      environment = {
        CPU_FOR_SECOND_THREAD = 1
      }
      additional_policies = []
    }
  }
}


module "generic-playground-lambda" {
  source = "./lambda"

  function_base_name = "generic-playground-static"
  filename           = "${local.lambda_build_dir}/buzz-${var.generic_playground_file}-static.zip"
  handler            = "N/A"
  memory_size        = local.playground[var.generic_playground_file].memory_size
  timeout            = 10
  runtime            = "provided"

  additional_policies = local.playground[var.generic_playground_file].additional_policies

  environment = merge(
    {
      BUILD_FILE = var.generic_playground_file
    },
    local.playground[var.generic_playground_file].environment
  )
}
