FROM amazonlinux:1

RUN yum update -y && yum install -y unzip

COPY buzz-test1-static.zip .
# COPY buzz-test1-shared.zip .

ENV LD_LIBRARY_PATH lib
ENV AWS_REGION eu-west-1
ENV IS_LOCAL true

ENTRYPOINT unzip buzz-test1-${BUILD_TYPE}.zip && \
  bin/buzz-test1-${BUILD_TYPE} N/A
