#!/bin/sh
set -e

 # These grow the executable size (5MB -> 30MB) but don't seem to impact perf
 # -ldl flag necessary for stacktrace
 # -g retain source level info

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
    -DARROW_FILESYSTEM=ON \
    -DARROW_WITH_ZLIB=ON \
    -DARROW_FLIGHT=ON \
    -DCMAKE_PREFIX_PATH=/install \
    -DBUZZ_BUILD_FILE=${BUILD_FILE} \
    -DBUZZ_BUILD_TYPE=${BUILD_TYPE} \
    -DBUZZ_BUILD_TESTS=OFF
  make
elif [ "$1" = 'package' ]; then
  cd /build
  /packager -d /build/executables/buzz-${BUILD_FILE}-${BUILD_TYPE}
elif [ "$1" = 'test' ]; then
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
    -DARROW_FILESYSTEM=ON \
    -DARROW_WITH_ZLIB=ON \
    -DARROW_FLIGHT=ON \
    -DCMAKE_PREFIX_PATH=/install \
    -DBUZZ_BUILD_TYPE=${BUILD_TYPE} \
    -DBUZZ_BUILD_TESTS=ON
  make
  ctest --verbose
else
  exec "$@"
fi

