FROM buzz-amznlinux1-build-cpp

RUN yum update  -y && \
  yum install -y zip && \
  yum clean all


COPY --from=buzz-lambda-runtime-cpp /install /install
COPY --from=buzz-aws-sdk-cpp /install /install

ENV ARROW_VERSION 0.17.0

RUN git clone -b apache-arrow-${ARROW_VERSION} --depth 1 https://github.com/apache/arrow.git /source 

# patch arrow CMakeLists to build buzz
RUN echo -e "add_subdirectory(buzz)\n" >> /source/cpp/CMakeLists.txt
RUN echo $'\
if(ARROW_S3)\n\
  list(APPEND ARROW_STATIC_LINK_LIBS ${AWSSDK_LINK_LIBRARIES})\n\
endif()\n' \
  >> tmp_static_sdk_injection && \
  mv /source/cpp/src/arrow/CMakeLists.txt tmp_static_original && \
  cat tmp_static_sdk_injection > /source/cpp/src/arrow/CMakeLists.txt && \
  cat tmp_static_original >> /source/cpp/src/arrow/CMakeLists.txt && \
  rm tmp_static_sdk_injection tmp_static_original

COPY code /source/cpp/buzz

COPY docker/arrow-cpp/docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["build"]