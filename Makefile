GIT_REVISION = `git rev-parse --short HEAD``git diff --quiet HEAD -- || echo "-dirty"`
PROFILE = $(eval PROFILE := $(shell bash -c 'read -p "Profile: " input; echo $$input'))$(PROFILE)
STAGE = $(eval STAGE := $(shell bash -c 'read -p "Stage: " input; echo $$input'))$(STAGE)
SHELL := /bin/bash # Use bash syntax
REGION := eu-west-1

## global commands

check-dirty:
	@git diff --quiet HEAD -- || { echo "ERROR: commit first, or use 'make force-deploy' to deploy dirty"; exit 1; }

ask-target:
	@echo "Lets deploy ${GIT_REVISION} in ${STAGE} with profile ${PROFILE}..."

bin-folder:
	@mkdir -p bin/build-amznlinux1
	@mkdir -p bin/build-ubuntu
	# @mkdir -p bin/build-bench

# requires with AWS CLI v2
docker-login:
	aws ecr get-login-password --region "${REGION}" --profile=${PROFILE} | \
	docker login --username AWS --password-stdin \
		"$(shell aws sts get-caller-identity --profile=${PROFILE} --query 'Account' --output text).dkr.ecr.${REGION}.amazonaws.com"

## build commands
build-lambda-runtime-cpp:
	cd docker/lambda-runtime-cpp && \
	docker build -t buzz-lambda-runtime-cpp .

arrow-cpp-hive-build-image: bin-folder
	cd docker/aws-sdk-cpp && \
	docker build \
		-t buzz-aws-sdk-cpp-ubuntu \
		--build-arg PLATFORM=cloudfuse/ubuntu-builder:gcc75 \
		.
	git submodule update --init
	docker build -f docker/arrow-cpp/hive.Dockerfile -t buzz-arrow-cpp-build-hive .
	# docker run -it buzz-arrow-cpp-build-hive sh

arrow-cpp-bee-build-image: bin-folder build-lambda-runtime-cpp
	cd docker/aws-sdk-cpp && \
	docker build \
		-t buzz-aws-sdk-cpp-amznlinux1 \
		--build-arg PLATFORM=cloudfuse/amazonlinux1-builder:gcc72 \
		.
	git submodule update --init
	docker build -f docker/arrow-cpp/bee.Dockerfile -t buzz-arrow-cpp-build-bee .
	# docker run -it buzz-arrow-cpp-build-bee bash

# arrow-cpp-bench-image: bin-folder
# 	docker build -t buzz-arrow-cpp-bench -f docker/arrow-cpp/benchmarks.Dockerfile .
# 	docker run --rm -it -v ${CURDIR}/bin/build-bench:/tmp/ buzz-arrow-cpp-bench

bin/build-amznlinux1/executables/buzz-%-static: arrow-cpp-bee-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-amznlinux1:/build \
		-e BUILD_FILE=$* \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build-bee \
		build

bin/build-amznlinux1/executables/buzz-%-static.zip: bin/build-amznlinux1/executables/buzz-%-static
	docker run --rm \
		-v ${CURDIR}/bin/build-amznlinux1/executables:/build/executables \
		-e BUILD_FILE=$* \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build-bee \
		package

build-bee:
	make bin/build-amznlinux1/executables/buzz-${BUILD_FILE}-static

package-bee:
	make bin/build-amznlinux1/executables/buzz-${BUILD_FILE}-static.zip

build-hive: arrow-cpp-hive-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-ubuntu:/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build-hive \
		build

test: arrow-cpp-bee-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-tests:/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build-bee \
		test

## local bee run commands

# possible values: 
# - emulator: run zipped package in a clean amazonlinux1 env
# - builder: run compiled exec directly in builder
RUNTIME ?= builder

compose-clean-run-bee:
	@docker-compose \
		-f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml \
		-f docker/amznlinux1-run-cpp/docker-compose.${RUNTIME}.yaml \
		build --parallel
	docker-compose \
		-f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml \
		-f docker/amznlinux1-run-cpp/docker-compose.${RUNTIME}.yaml \
		up --abort-on-container-exit
	docker logs amznlinux1-run-cpp_lambda-runtime_1
	@docker-compose \
		-f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml \
		-f docker/amznlinux1-run-cpp/docker-compose.${RUNTIME}.yaml \
		rm -fsv

VALGRIND_CMD ?= ""
# VALGRIND_CMD="valgrind --leak-check=yes"
# VALGRIND_CMD="valgrind --pages-as-heap=yes --tool=massif"
# docker cp amznlinux1-run-cpp_lambda-emulator_1:/massif.out.1 massif.out.1

run-bee-local:
ifeq ($(RUNTIME),emulator)
	BUILD_FILE=${BUILD_FILE} make package-bee
else
	BUILD_FILE=${BUILD_FILE} make build-bee
endif
	CURDIR=${CURDIR} \
	VALGRIND_CMD="${VALGRIND_CMD}" \
	COMPOSE_TYPE=${COMPOSE_TYPE} \
	BUILD_FILE=${BUILD_FILE} \
	make compose-clean-run-bee

run-local-query-bandwidth: 
	COMPOSE_TYPE=minio \
	BUILD_FILE=query-bandwidth \
	make run-bee-local

run-local-query-bandwidth2: 
	COMPOSE_TYPE=minio \
	BUILD_FILE=query-bandwidth2 \
	make run-bee-local

run-local-parquet-arrow-reader:
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-arrow-reader \
	make run-bee-local

run-local-parquet-raw-reader:
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-raw-reader \
	make run-bee-local

run-local-parquet-raw-reader2:
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-raw-reader2 \
	make run-bee-local

run-local-mem-alloc-overprov:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-overprov \
	make run-bee-local

run-local-mem-alloc-speed:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-speed \
	make run-bee-local

run-local-simd-support:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=simd-support \
	make run-bee-local

run-local-mem-bandwidth:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-bandwidth \
	make run-bee-local

run-local-raw-alloc:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=raw-alloc \
	make run-bee-local

run-local-core-affinity:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=core-affinity \
	make run-bee-local

bash-inside-emulator:
	BUILD_FILE=${BUILD_FILE} docker-compose \
		-f docker/amznlinux1-run-cpp/docker-compose.standalone.yaml \
		-f docker/amznlinux1-run-cpp/docker-compose.emulator.yaml \
		-f docker/amznlinux1-run-cpp/docker-compose.interactive.yaml \
		run --rm lambda-runtime 

## local hive run commands

dockerify-hive: build-hive
	docker-compose \
		-f docker/ubuntu-run-cpp/docker-compose.yaml \
		build

run-hive-local: dockerify-hive
	docker-compose \
		-f docker/ubuntu-run-cpp/docker-compose.yaml \
		up --abort-on-container-exit
	docker-compose \
		-f docker/ubuntu-run-cpp/docker-compose.yaml \
		rm -fsv

run-local-flight-server:
	BUILD_FILE=flight-server \
	make run-hive-local

run-local-query-bw-scheduler:
	BUILD_FILE=query-bw-scheduler \
	AWS_PROFILE=${AWS_PROFILE} \
	IMAGE_TAG="${GIT_REVISION}" \
	make run-hive-local

## bee deployment commands

# this defaults the file deployed to the generic playground
GEN_PLAY_FILE ?= query-bandwidth2

deploy-bee:
	BUILD_FILE=${GEN_PLAY_FILE} make package-bee
	@cd infra; terraform workspace select dev
	cd infra; terraform apply \
		-target=module.generic-playground-lambda \
		-auto-approve \
		--var profile=bbdev \
		--var git_revision=${GIT_REVISION} \
		--var generic_playground_file=${GEN_PLAY_FILE}

run-bee:
	@aws lambda invoke \
		--function-name buzz-cpp-generic-playground-static-dev \
		--log-type Tail \
		--region ${REGION} \
		--profile bbdev \
		--query 'LogResult' \
		--output text \
		/dev/null | base64 -d

deploy-run-bee: deploy-bee
	sleep 2
	make run-bee

deploy-bench-query-bandwidth2:
	GEN_PLAY_FILE=query-bandwidth2 make deploy-bee 
	@# change the unused param "handler" to reset lambda state
	number=1 ; while [[ $$number -le 1 ]] ; do \
		aws lambda update-function-configuration \
			--function-name buzz-cpp-generic-playground-static-dev \
			--handler "N/A-$$number" \
			--region ${REGION} \
			--profile bbdev  > /dev/null 2>&1; \
		make run-bee 2>&- | grep '^{.*query_bandwidth2.*}$$' | jq -r '[.speed_MBpS, .MAX_PARALLEL, .run]|@csv'; \
		make run-bee 2>&- | grep '^{.*query_bandwidth2.*}$$' | jq -r '[.speed_MBpS, .MAX_PARALLEL, .run]|@csv'; \
		make run-bee 2>&- | grep '^{.*query_bandwidth2.*}$$' | jq -r '[.speed_MBpS, .MAX_PARALLEL, .run]|@csv'; \
		make run-bee 2>&- | grep '^{.*query_bandwidth2.*}$$' | jq -r '[.speed_MBpS, .MAX_PARALLEL, .run]|@csv'; \
		((number = number + 1)) ; \
	done

deploy-bench-parquet-raw-reader2:
	GEN_PLAY_FILE=parquet-raw-reader2 make deploy-bee 
	@# change the unused param "handler" to reset lambda state
	number=1 ; while [[ $$number -le 25 ]] ; do \
		aws lambda update-function-configuration \
			--function-name buzz-cpp-generic-playground-static-dev \
			--handler "N/A-$$number" \
			--region ${REGION} \
			--profile bbdev  > /dev/null 2>&1; \
		make run-bee 2>&- | grep '^{.*wait_s3.*}$$' | jq -r '[.nb_init, .footer, .dl, .total, .run]|@csv'; \
		make run-bee 2>&- | grep '^{.*wait_s3.*}$$' | jq -r '[.nb_init, .footer, .dl, .total, .run]|@csv'; \
		((number = number + 1)) ; \
	done

# | grep '^{.*query_bandwidth2.*}$' | jq -r '[.speed_MBpS, .MAX_PARALLEL, .CONTAINER_RUNS]|@csv'

## hive deployment commands

# deploy-hive:
# 	BUILD_FILE=query-bw-scheduler IMAGE_TAG="${GIT_REVISION}" make dockerify-hive
# 	@cd infra; terraform workspace select dev
# 	cd infra; terraform apply \
# 		-target=module.query-bw-scheduler-fargate \
# 		-auto-approve \
# 		--var profile=bbdev \
# 		--var git_revision=${GIT_REVISION} \
# 		--var generic_playground_file=${GEN_PLAY_FILE}

run-hive:
	@cd infra; terraform workspace select ${ENV}
	aws ecs run-task \
		--cluster buzz-cpp-cluster-${ENV} \
		--count 1 \
		--region ${REGION} \
		--profile ${APPLY_PROFILE} \
		--task-definition ${BUILD_FILE}-static-${ENV} \
		--query 'failures' \
		--network-configuration "awsvpcConfiguration={\
			subnets=$(shell cd infra;terraform output subnet_ids),\
			securityGroups=[$(shell cd infra;terraform output query-bw-scheduler-fargate_security_group_id)],\
			assignPublicIp=ENABLED\
			}"

run-query-bw-scheduler:
	ENV=dev APPLY_PROFILE=bbdev BUILD_FILE=query-bw-scheduler make run-hive

# deploy-run-hive: deploy-hive
# 	sleep 2
# 	make run-hive

## deploy all commands

init-dev:
	cd infra; terraform workspace select dev
	cd infra; terraform init

force-deploy:
	BUILD_FILE=query-bw-scheduler IMAGE_TAG="${GIT_REVISION}" make dockerify-hive
	BUILD_FILE=query-bandwidth2 make package-bee
	BUILD_FILE=mem-bandwidth make package-bee
	BUILD_FILE=${GEN_PLAY_FILE} make package-bee
	@echo "DEPLOYING ${GIT_REVISION} to ${ENV} ..."
	@cd infra; terraform workspace select ${ENV}
	@cd infra; terraform apply \
		--var profile=${APPLY_PROFILE} \
		--var git_revision=${GIT_REVISION} \
		--var generic_playground_file=${GEN_PLAY_FILE}
	@echo "${GIT_REVISION} DEPLOYED !!!"

force-deploy-dev:
	ENV=dev APPLY_PROFILE=bbdev make force-deploy
