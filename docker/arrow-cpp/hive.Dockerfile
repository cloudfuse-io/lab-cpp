FROM cloudfuse/ubuntu-builder:gcc75

COPY --from=cloudfuse-lab-aws-sdk-cpp-ubuntu /install /install

COPY code /source

COPY docker/arrow-cpp/arrow /source/arrow

COPY docker/arrow-cpp/hive-entrypoint.sh /
ENTRYPOINT ["/hive-entrypoint.sh"]
CMD ["build"]