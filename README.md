# :test_tube: Container for Cloudfuse C++ experimentations :test_tube:

## Context

These experimentations where conducted with the following goals in mind:
- Familiarization with the Arrow C++ code base
- Evaluation of the effort required to build a query engine on top of it
- Try to deploy a C++ app with complex dependency tree on AWS Lambda
- Experiment on AWS Lambda raw performance (S3 bandidth, memory bandwidth, CPU instruction sets...)

## Vocabulary

The Cloudfuse query engine is splitted into two components:
- :bee: bees : the cloud function workers (AWS Lambda) that load and pre-aggregate the S3 data
- :honey_pot: hives: the containers (AWS Fargate) that collect and reduce the aggregates sent by bees.

## Structure

Deployments and runs are managed by the Makefile. Commands are detailed in the HOWTO section below.
- The `code/` directory contains the C++ code base. It contains a set of experiments that can be run individually in `code/playground/`. The rest of the code are helpers that are common to multiple experiments.
- The `data/` directory is the one mounted by minio to simulate S3 locally. Parquet files for experiments go here with the following path: `data/bucketname/keyname`.
- The `docker/` directory contains build imagesand scripts for the various dependencies of the project
- The `infra/` directory contains terraform scripts to deploy the experiments in the AWS cloud. This infra deploys a "generic" Lambda where you can load any of the experiment from `code/playground/`, lambdas with bandwidth tests automatically triggered to get statistics through time, and an ECS cluster with the hive config.

## HOWTO

### Prerequisits
- docker
- AWS account(s) + AWS CLI
- bash + makefile
- good knowledge of terraform, Cpp, cmake and docker

### Make commands

#### Local
- `make test` run C++ tests locally
- `make run-local-XXX` where XXX should be replaced by the experiment file name will run that experiment locally.
  - Note: `make run-local-flight-server` dependency to abseil seems broken 
- `make bash-inside-emulator` to explore the Lambda runtime emulator interactively

#### Remote
You need an AWS account with the AWS CLI and `.aws/credentials` properly configured. If you use S3 as a backend for terraform, you can use a bucket in a different account from your deployment (the account is determined by the `profile=xxx` config below). You first need to init your terraform remote backend:
```bash
cd infra 
terraform init \
			-backend-config="bucket=s3-tf-backend-bucket" \
			-backend-config="key=cloudfuse-labs-cpp" \
			-backend-config="region=eu-west-1" \
			-backend-config="profile=s3-tf-backend-profile"
terraform workspace new dev
cd ..
```
- `make init` run terraform init in the current workspace
- `GEN_PLAY_FILE=XXX make deploy-bee` deploy the experiment file XXX to the "generic" Lambda function. For lambdas that need to access an object from S3, you can configure it in `infra/playgroun-generic.tf`
- `make run-bee` run the experiment deployed in the "generic" Lambda function.
- `GEN_PLAY_FILE=XXX make deploy-run-bee` run both above
- `make deploy-bench-XXX` run functions multiple time stimulating cold starts by changing the function between runs
- `make docker-login` required to login to your ECR repository if deploying hive components to the cloud. You will be prompted for the AWS profile you want to use.
- `force-deploy` deploy the "generic" lambda for individual tests, nd bandwidth tests triggered by crons to get statistics of lambda perfs through time and a hive infra (you should configure a valid object in `infra/playgroun-collect.tf`). Needs docker-login to deploy hive. You will be prompted for the AWS profile you want to use.
- `make destroy` remove the resources. You will be prompted for the AWS profile that has access to the resources you want to destroy.

### Add backtrace

In the file where you want to print the backtrace:
```c++
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
```
```c++
std::cout << boost::stacktrace::stacktrace();
```

### Solve boost dependency issues

In the ThirdpartyToolchain.cmake file of arrow, remove slim boost source URLs:
```bash
  set_urls(
    BOOST_SOURCE_URL
    # These are trimmed boost bundles we maintain.
    # See cpp/build_support/trim-boost.sh
    # "https://dl.bintray.com/ursalabs/arrow-boost/boost_${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}.tar.gz"
    "https://dl.bintray.com/boostorg/release/${ARROW_BOOST_BUILD_VERSION}/source/boost_${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}.tar.gz"
    "https://github.com/boostorg/boost/archive/boost-${ARROW_BOOST_BUILD_VERSION}.tar.gz"
    # FIXME(ARROW-6407) automate uploading this archive to ensure it reflects
    # our currently used packages and doesn't fall out of sync with
    # ${ARROW_BOOST_BUILD_VERSION_UNDERSCORES}
    # 
  )
```
