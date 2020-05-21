#!/bin/bash
set -e

 # -ldl flag necessary for stacktrace

if [ "$1" = 'run' ]; then
    cd /source/docker/arrow-cpp/arrow
    archery benchmark run --benchmark-filter=MinioFixture/ReadAll500Mib/real_time
else
    exec "$@"
fi
