FROM amazonlinux:1

RUN yum update  -y && \
  yum install -y \
    curl-devel \
    gcc \
    gcc-c++ \
    git \
    ninja-build \
    openssl-devel && \
  yum clean all

ENV CMAKE_VERSION_MAJOR 3.17
ENV CMAKE_VERSION 3.17.1

RUN curl https://cmake.org/files/v${CMAKE_VERSION_MAJOR}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz --output cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz && \
  tar -zxvf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz && \
  mv cmake-${CMAKE_VERSION}-Linux-x86_64 /opt && \
  rm cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz && \
  ln -s /opt/cmake-${CMAKE_VERSION}-Linux-x86_64/bin/cmake /usr/local/bin/cmake
