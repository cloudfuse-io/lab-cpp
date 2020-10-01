FROM amazonlinux:1

RUN yum update  -y && \
  yum install -y unzip && \
  yum clean all

ARG BUILD_FILE
ARG BUILD_TYPE=static

COPY cloudfuse-lab-${BUILD_FILE}-${BUILD_TYPE}.zip .

RUN unzip cloudfuse-lab-${BUILD_FILE}-${BUILD_TYPE}.zip &&\
  mv bin/cloudfuse-lab-${BUILD_FILE}-${BUILD_TYPE} bin/exec

ENTRYPOINT bin/exec N/A
