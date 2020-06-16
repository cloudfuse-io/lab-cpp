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
		-v ${CURDIR}/bin/build-amznlinux1:/source/cpp/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build \
		build package

test: arrow-cpp-build-image
	docker run --rm \
		-v ${CURDIR}/bin/build-amznlinux1:/source/cpp/build \
		-e BUILD_FILE=${BUILD_FILE} \
		-e BUILD_TYPE=static \
		buzz-arrow-cpp-build \
		build test

## deployment commands

compose-clean-run:
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml build
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml up --abort-on-container-exit
	docker logs amznlinux1-run-cpp_lambda-emulator_1
	# docker cp amznlinux1-run-cpp_lambda-emulator_1:/massif.out.1 massif.out.1
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.${COMPOSE_TYPE}.yaml rm -fsv

# VALGRIND_CMD="valgrind --leak-check=yes"
# VALGRIND_CMD="valgrind --pages-as-heap=yes --tool=massif"

run-local-query-bandwidth: 
	BUILD_FILE=query-bandwidth make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=minio \
	BUILD_FILE=query-bandwidth \
	make compose-clean-run

run-local-parquet-reader:
	BUILD_FILE=parquet-reader make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=minio \
	BUILD_FILE=parquet-reader \
	make compose-clean-run

run-local-mem-alloc-overprov:
	BUILD_FILE=mem-alloc-overprov make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-overprov \
	make compose-clean-run

run-local-mem-alloc-speed:
	BUILD_FILE=mem-alloc-speed make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=standalone \
	BUILD_FILE=mem-alloc-speed \
	make compose-clean-run

run-local-simd-support:
	BUILD_FILE=simd-support make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=standalone \
	BUILD_FILE=simd-support \
	make compose-clean-run

run-local-raw-alloc:
	BUILD_FILE=raw-alloc make build
	VALGRIND_CMD="" \
	COMPOSE_TYPE=standalone \
	BUILD_FILE=raw-alloc \
	make compose-clean-run

# temp command:
force-deploy-dev:
	# BUILD_FILE=query-bandwidth make build
	BUILD_FILE=parquet-reader make build
	# BUILD_FILE=mem-alloc-overprov make build
	# BUILD_FILE=mem-alloc-speed make build
	# BUILD_FILE=simd-support make build
	# BUILD_FILE=raw-alloc make build
	@echo "DEPLOYING ${GIT_REVISION} to dev ..."
	@cd infra; terraform workspace select dev
	@cd infra; terraform apply --var profile=bbdev --var git_revision=${GIT_REVISION}
	@echo "${GIT_REVISION} DEPLOYED !!!"

