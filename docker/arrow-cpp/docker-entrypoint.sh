#!/bin/bash
set -e

if [ "$1" = 'build' ]; then
    mkdir -p /source/cpp/build
    cd /source/cpp/build
    cmake .. -DCMAKE_BUILD_TYPE=Release \
      -DARROW_BUILD_SHARED=ON \
      -DARROW_PARQUET=ON \
      -DARROW_S3=ON \
      -DARROW_FILESYSTEM=ON \
      -DARROW_WITH_ZLIB=ON \
      -DCMAKE_PREFIX_PATH=/install \
      -DBUZZ_BUILD_FILE=${BUILD_FILE} \
      -DBUZZ_BUILD_TYPE=${BUILD_TYPE}
    make
    make aws-lambda-package-buzz-${BUILD_FILE}-${BUILD_TYPE}
else
    exec "$@"
fi
