ARG PLATFORM
FROM ${PLATFORM}

ENV SDK_VERSION 1.8.4

RUN git clone -b ${SDK_VERSION} --depth 1 https://github.com/aws/aws-sdk-cpp.git /source

RUN mkdir -p /source/build && \
  cd /source/build && \
  cmake .. \
  -DBUILD_ONLY="config;s3;transfer" \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_CXX_FLAGS="-w" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/install \
  -DCUSTOM_MEMORY_MANAGEMENT=OFF \
  -DENABLE_UNITY_BUILD=ON \
  -DENABLE_TESTING=OFF && \
  make && \
  make install

CMD ["bash"]