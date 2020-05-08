FROM amazonlinux:1

RUN yum update -y && yum install -y unzip

ARG BUILD_FILE
ARG BUILD_TYPE

COPY buzz-${BUILD_FILE}-${BUILD_TYPE}.zip .
# COPY buzz-test1-shared.zip .

ENV LD_LIBRARY_PATH lib
ENV AWS_REGION eu-west-1
ENV IS_LOCAL true

RUN unzip buzz-${BUILD_FILE}-${BUILD_TYPE}.zip &&\
  mv bin/buzz-${BUILD_FILE}-${BUILD_TYPE} bin/exec

ENTRYPOINT bin/exec N/A
