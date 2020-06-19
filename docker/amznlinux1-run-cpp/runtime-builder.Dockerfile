FROM amazonlinux:1

RUN yum update -y && yum install -y unzip valgrind

ENV AWS_REGION eu-west-1
ENV IS_LOCAL true

ENTRYPOINT ${VALGRIND_CMD} /release/buzz-${BUILD_FILE}-static
