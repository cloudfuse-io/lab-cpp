FROM cloudfuse/amazonlinux1-builder:gcc72

COPY --from=buzz-lambda-runtime-cpp /install /install
COPY docker/arrow-cpp/packager /packager
COPY --from=buzz-aws-sdk-cpp-amznlinux1 /install /install

COPY code /source

COPY docker/arrow-cpp/arrow /source/arrow

COPY docker/arrow-cpp/bee-entrypoint.sh /
ENTRYPOINT ["/bee-entrypoint.sh"]
CMD ["build"]