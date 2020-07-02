#!/bin/sh
set -e

if [ "$1" = 'build' ]; then
    mkdir -p /build
    cd /build
    cmake /source -DCMAKE_BUILD_TYPE=Release \
      -DARROW_BUILD_STATIC=ON \
      -DARROW_BUILD_SHARED=OFF \
      -DARROW_BUILD_TESTS=OFF \
      -DARROW_CXXFLAGS="-ldl -g" \
      -DARROW_DEPENDENCY_SOURCE=AUTO \
      -DARROW_JEMALLOC=ON \
      -DARROW_PARQUET=ON \
      -DARROW_JSON=ON \
      -DARROW_S3=ON \
      -DARROW_FILESYSTEM=ON \
      -DARROW_FLIGHT=ON \
      -DARROW_WITH_ZLIB=ON \
      -DBUZZ_BUILD_FILE=${BUILD_FILE} \
      -DBUZZ_BUILD_TYPE=${BUILD_TYPE} \
      -DBUZZ_BUILD_TESTS=${BUILD_TESTS}
    # -DCMAKE_PREFIX_PATH=/install \
    make
else
    exec "$@"
fi
