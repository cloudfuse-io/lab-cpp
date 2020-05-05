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
      -DCMAKE_PREFIX_PATH=/install
    make
    # make aws-lambda-package-buzz-test1-shared
    make aws-lambda-package-buzz-test1-static
else
    exec "$@"
fi
