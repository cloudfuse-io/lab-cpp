ARG arch=amd64
FROM ${arch}/ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

# Installs LLVM toolchain, for Gandiva and testing other compilers
# Too many OOMs with ninja-build
ARG clang_tools=8
RUN apt-get update -y -q \
    && apt-get install -y -q \
    clang-${clang_tools} \
    clang-format-${clang_tools} \
    clang-tidy-${clang_tools} \
    clang-tools-${clang_tools} \
    libclang-${clang_tools}-dev \
    llvm-${clang_tools}-dev \
    autoconf \
    ca-certificates \
    ccache \
    cmake \
    g++ \
    gcc \
    gdb \
    git \
    curl \
    python3 \
    python3-dev \
    python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*


RUN ln -s /usr/bin/python3 /usr/local/bin/python && \
    ln -s /usr/bin/pip3 /usr/local/bin/pip

COPY . /source

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN pip install wheel setuptools && pip install -e /source/docker/arrow-cpp/arrow/dev/archery

ENTRYPOINT ["/source/docker/arrow-cpp/benchmarks-entrypoint.sh"]
CMD ["run"]

# cd /source/docker/arrow-cpp/arrow && archery benchmark diff --preserve