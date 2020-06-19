FROM amazonlinux:1

RUN yum update -y && yum install -y unzip valgrind

ARG BUILD_FILE
ARG BUILD_TYPE=static

COPY buzz-${BUILD_FILE}-${BUILD_TYPE}.zip .
# COPY buzz-test1-shared.zip .

ENV AWS_REGION eu-west-1
ENV IS_LOCAL true

RUN unzip buzz-${BUILD_FILE}-${BUILD_TYPE}.zip &&\
  mv bin/buzz-${BUILD_FILE}-${BUILD_TYPE} bin/exec

ENTRYPOINT ${VALGRIND_CMD} bin/exec N/A
