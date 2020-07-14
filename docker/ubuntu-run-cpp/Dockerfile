FROM ubuntu:18.04

ARG BUILD_FILE
ARG BUILD_TYPE=static

# autoconf \
# ca-certificates \
# ccache \
# g++ \
# gcc \
# gdb \
# git \
# curl \
# libcurl4-openssl-dev \
# libssl-dev \
# ninja-build \
# pkg-config \
# zlib1g-dev \

RUN apt-get update -y -q \
  && apt-get install -y -q \
  libcurl4-openssl-dev \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY buzz-${BUILD_FILE}-${BUILD_TYPE} /app

ENTRYPOINT ["/app"]