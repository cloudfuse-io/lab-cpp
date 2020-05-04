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
	@mkdir -p bin

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

build-arrow-cpp: build-lambda-runtime-cpp build-aws-sdk-cpp
	git submodule update --init
	docker build -f docker/arrow-cpp/Dockerfile -t buzz-arrow-cpp-build .
	docker run -v ${CURDIR}/bin/build:/source/cpp/build buzz-arrow-cpp-build
	# docker run -it buzz-arrow-cpp-build bash

## deployment commands

run-local-arrow-cpp: build-arrow-cpp
	docker build -f docker/amznlinux1-run-cpp/Dockerfile -t buzz-amznlinux1-run-cpp  bin/build/buzz
	# /!\ following does not work with role_arn profile, need to be profile with long term creds
	docker run -v ~/.aws:/root/.aws -e AWS_PROFILE=bbdev -e BUILD_TYPE=shared buzz-amznlinux1-run-cpp

# temp command:
force-deploy-dev: build-arrow-cpp
	@echo "DEPLOYING ${GIT_REVISION} to dev ..."
	@cd infra; terraform workspace select dev
	@cd infra; terraform apply --var profile=bbdev --var git_revision=${GIT_REVISION}
	@echo "${GIT_REVISION} DEPLOYED !!!"

