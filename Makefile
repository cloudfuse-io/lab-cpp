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
	@mkdir -p bin/build

## build commands

build-amznlinux1-build-cpp:
	cd docker/amznlinux1-build-cpp && \
	docker build -t buzz-amznlinux1-build-cpp .

build-lambda-runtime-cpp: build-amznlinux1-build-cpp
	cd docker/lambda-runtime-cpp && \
	docker build -t buzz-lambda-runtime-cpp .

build-aws-sdk-cpp: build-amznlinux1-build-cpp
	cd docker/aws-sdk-cpp && \
	docker build -t buzz-aws-sdk-cpp .

arrow-cpp-build-image: bin-folder build-lambda-runtime-cpp build-aws-sdk-cpp
	git submodule update --init
	docker build -f docker/arrow-cpp/Dockerfile -t buzz-arrow-cpp-build .
	# docker run -it buzz-arrow-cpp-build bash

build-query-bandwidth: arrow-cpp-build-image
	docker run --rm -v ${CURDIR}/bin/build:/source/cpp/build -e BUILD_FILE=query-bandwidth -e BUILD_TYPE=static buzz-arrow-cpp-build

build-parquet-reader: arrow-cpp-build-image
	docker run --rm -v ${CURDIR}/bin/build:/source/cpp/build -e BUILD_FILE=parquet-reader -e BUILD_TYPE=static buzz-arrow-cpp-build

## deployment commands

compose-clean-run:
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.yaml build
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.yaml up --abort-on-container-exit
	docker-compose -f docker/amznlinux1-run-cpp/docker-compose.yaml rm -fsv

run-local-query-bandwidth: build-query-bandwidth
	docker build \
	  --build-arg BUILD_FILE=query-bandwidth \
	  --build-arg BUILD_TYPE=static \
	  -f docker/amznlinux1-run-cpp/runtime.Dockerfile \
	  -t buzz-amznlinux1-run-cpp \
	  bin/build/buzz
	make compose-clean-run

run-local-parquet-reader: build-parquet-reader
	docker build \
	  --build-arg BUILD_FILE=parquet-reader \
	  --build-arg BUILD_TYPE=static \
	  -f docker/amznlinux1-run-cpp/runtime.Dockerfile \
	  -t buzz-amznlinux1-run-cpp \
	  bin/build/buzz
	make compose-clean-run

# temp command:
force-deploy-dev: build-query-bandwidth build-parquet-reader
	@echo "DEPLOYING ${GIT_REVISION} to dev ..."
	@cd infra; terraform workspace select dev
	@cd infra; terraform apply --var profile=bbdev --var git_revision=${GIT_REVISION}
	@echo "${GIT_REVISION} DEPLOYED !!!"

