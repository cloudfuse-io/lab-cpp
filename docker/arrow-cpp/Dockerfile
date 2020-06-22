FROM cloudfuse/amazonlinux1-builder:gcc72

RUN yum update  -y && \
  yum install -y zip && \
  yum clean all


COPY --from=buzz-lambda-runtime-cpp /install /install
COPY docker/arrow-cpp/packager /install/lib/aws-lambda-runtime/cmake/packager
COPY --from=buzz-aws-sdk-cpp /install /install

COPY code /source

COPY docker/arrow-cpp/arrow /source/arrow

RUN echo $'\
if(ARROW_S3)\n\
  list(APPEND ARROW_STATIC_LINK_LIBS ${AWSSDK_LINK_LIBRARIES})\n\
endif()\n' \
  >> tmp_static_sdk_injection && \
  mv /source/arrow/cpp/src/arrow/CMakeLists.txt tmp_static_original && \
  cat tmp_static_sdk_injection > /source/arrow/cpp/src/arrow/CMakeLists.txt && \
  cat tmp_static_original >> /source/arrow/cpp/src/arrow/CMakeLists.txt && \
  rm tmp_static_sdk_injection tmp_static_original

COPY docker/arrow-cpp/docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["build"]