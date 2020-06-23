#!/bin/bash
set -e

 # These grow the executable size (5MB -> 30MB) but don't seem to impact perf
 # -ldl flag necessary for stacktrace
 # -g retain source level info
BUILD_TESTS=OFF
if [ "$2" = 'test' ]; then
 BUILD_TESTS=ON
fi

if [ "$1" = 'build' ]; then
    mkdir -p /build
    cd /build
    cmake /source -DCMAKE_BUILD_TYPE=Release \
      -DARROW_BUILD_STATIC=ON \
      -DARROW_DEPENDENCY_SOURCE=AUTO \
      -DARROW_CXXFLAGS="-ldl -g" \
      -DARROW_BUILD_TESTS=OFF \
      -DARROW_BUILD_SHARED=OFF \
      -DARROW_JEMALLOC=ON \
      -DARROW_PARQUET=ON \
      -DARROW_JSON=ON \
      -DARROW_S3=ON \
      -DARROW_FILESYSTEM=ON \
      -DARROW_WITH_ZLIB=ON \
      -DCMAKE_PREFIX_PATH=/install \
      -DBUZZ_BUILD_FILE=${BUILD_FILE} \
      -DBUZZ_BUILD_TYPE=${BUILD_TYPE} \
      -DBUZZ_BUILD_TESTS=${BUILD_TESTS}
    make
else
    exec "$@"
fi

if [ "$2" = 'package' ]; then
 make aws-lambda-package-buzz-${BUILD_FILE}-${BUILD_TYPE}
elif [ "$2" = 'test' ]; then
 cd /build
 ctest --verbose
fi
