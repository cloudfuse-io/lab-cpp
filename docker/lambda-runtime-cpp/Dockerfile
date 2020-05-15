FROM cloudfuse/amazonlinux1-cpp-builder:latest

ENV LAMBDA_RUNTIME_VERSION 0.2.6

RUN git clone -b v${LAMBDA_RUNTIME_VERSION} --depth 1 https://github.com/awslabs/aws-lambda-cpp.git /source

RUN mkdir -p /source/build && \
  cd /source/build && \
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/install && \
  make && \
  make install

CMD ["bash"]
