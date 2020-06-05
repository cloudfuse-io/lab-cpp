#!/bin/bash
set -e

 # -ldl flag necessary for stacktrace

if [ "$1" = 'build' ]; then
    mkdir -p /source/cpp/build
    cd /source/cpp/build
    cmake .. -DCMAKE_BUILD_TYPE=Release \
      -DARROW_CXXFLAGS="-ldl -g" \
      -DARROW_BUILD_SHARED=OFF \
      -DARROW_BUILD_TESTS=ON \
      -DARROW_JEMALLOC=ON \
      -DARROW_PARQUET=ON \
      -DARROW_S3=ON \
      -DARROW_FILESYSTEM=ON \
      -DARROW_WITH_ZLIB=ON \
      -DCMAKE_PREFIX_PATH=/install \
      -DBUZZ_BUILD_FILE=${BUILD_FILE} \
      -DBUZZ_BUILD_TYPE=${BUILD_TYPE}
    make
else
    exec "$@"
fi

if [ "$2" = 'package' ]; then
 make aws-lambda-package-buzz-${BUILD_FILE}-${BUILD_TYPE}
elif [ "$2" = 'test' ]; then
 cd /source/cpp/build/buzz
 /opt/cmake-3.17.1-Linux-x86_64/bin/ctest --verbose
fi
