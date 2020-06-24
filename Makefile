GIT_REVISION=`git rev-parse --short HEAD``git diff --quiet HEAD -- || echo "-dirty"`
PROFILE = $(eval PROFILE := $(shell bash -c 'read -p "Profile: " input; echo $$input'))$(PROFILE)
STAGE = $(eval STAGE := $(shell bash -c 'read -p "Stage: " input; echo $$input'))$(STAGE)

## global commands

dos2unix:
	find . -not -path "./vendor/*" -not -path "./.git/*" -type f -print0 | xargs -0 dos2unix

check-dirty:
	@git diff --quiet HEAD -- || { echo "ERROR: commit first, or use 'make force-deploy' to deploy dirty"; exit 1; }

ask-target:
	@echo "Lets deploy ${GIT_REVISION} in ${STAGE} with profile ${PROFILE}..."

bin-folder:
	@mkdir -p bin/build-amznlinux1
	@mkdir -p bin/build-bench

## build commands
build-lambda-runtime-cpp:
	cd docker/lambda-runtime-cpp && \
	docker build -t buzz-lambda-runtime-cpp .

build-aws-sdk-cpp:
	cd docker/aws-sdk-cpp && \
	docker build -t buzz-aws-sdk-cpp .

arrow-cpp-build-image: bin-folder build-lambda-runtime-cpp build-aws-sdk-cpp
	git submodule update --init
	docker build -f docker/arrow-cpp/Dockerfile -t buzz-arrow-cpp-build .
	# docker run -it buzz-arrow-cpp-build bash

arrow-cpp-bench-image: bin-folder
	docker build -t buzz-arrow-cpp-bench -f docker/arrow-cpp/benchmarks.Dockerfile .
	docker run --rm -it -v ${CURDIR}/bin/build-bench:/tmp/ buzz-arrow-cpp-bench

build: arrow-cpp-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-amznlinux1:/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build \
		build

package: arrow-cpp-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-amznlinux1:/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build \
		build package

test: arrow-cpp-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-tests:/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build \
		build test

## local run commands

# possible values: 
# - emulator: run zipped package in a clean amazonlinux1 env
# - builder: run compiled exec directly in builder
RUNTIME ?= builder

compose-clean-run:
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

# VALGRIND_CMD="valgrind --leak-check=yes"
# VALGRIND_CMD="valgrind --pages-as-heap=yes --tool=massif"
# docker cp amznlinux1-run-cpp_lambda-emulator_1:/massif.out.1 massif.out.1

run-local:
ifeq ($(RUNTIME),emulator)
	BUILD_FILE=${BUILD_FILE} make package
else
	BUILD_FILE=${BUILD_FILE} make build
endif
	CURDIR=${CURDIR} \
	VALGRIND_CMD="" \
	COMPOSE_TYPE=${COMPOSE_TYPE} \
	BUILD_FILE=${BUILD_FILE} \
	make compose-clean-run

run-local-query-bandwidth: 
	COMPOSE_TYPE=minio \
	BUILD_FILE=query-bandwidth \
	make run-local

run-local-parquet-arrow-reader:
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-arrow-reader \
	make run-local

run-local-parquet-raw-reader:
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-raw-reader \
	make run-local

run-local-mem-alloc-overprov:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-overprov \
	make run-local

run-local-mem-alloc-speed:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-speed \
	make run-local

run-local-simd-support:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=simd-support \
	make run-local

run-local-mem-bandwidth:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-bandwidth \
	make run-local

run-local-raw-alloc:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=raw-alloc \
	make run-local

run-local-core-affinity:
	COMPOSE_TYPE=standalone \
	BUILD_FILE=core-affinity \
	make run-local

## deployment commands

init-dev:
	cd infra; terraform init

force-deploy-dev-one:
	make package
	@cd infra; terraform workspace select dev
	cd infra; terraform apply -target=module.${BUILD_FILE}-lambda --var profile=bbdev --var git_revision=${GIT_REVISION} 

force-deploy-dev:
	BUILD_FILE=query-bandwidth make package
	BUILD_FILE=parquet-arrow-reader make package
	BUILD_FILE=parquet-raw-reader make package
	BUILD_FILE=mem-alloc-overprov make package
	BUILD_FILE=mem-alloc-speed make package
	BUILD_FILE=simd-support make package
	BUILD_FILE=mem-bandwidth make package
	BUILD_FILE=raw-alloc make package
	@echo "DEPLOYING ${GIT_REVISION} to dev ..."
	@cd infra; terraform workspace select dev
	@cd infra; terraform apply --var profile=bbdev --var git_revision=${GIT_REVISION}
	@echo "${GIT_REVISION} DEPLOYED !!!"

