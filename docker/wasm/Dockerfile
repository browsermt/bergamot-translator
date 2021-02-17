FROM emscripten/emsdk:2.0.9

# Install specific version of CMake
WORKDIR /usr
RUN wget https://github.com/Kitware/CMake/releases/download/v3.17.2/cmake-3.17.2-Linux-x86_64.tar.gz -qO-\
    | tar xzf - --strip-components 1

# Install Python and Java (needed for Closure Compiler minification)
RUN apt-get update \
    && apt-get install -y \
    python3 \
    default-jre

# Deps to compile protobuf from source + the protoc binary which we need natively
RUN apt-get update -y  && apt-get --no-install-recommends -y install \
    protobuf-compiler \
    autoconf \
    autotools-dev \
    automake \
    autogen \
    libtool && ln -s /usr/bin/libtoolize /usr/bin/libtool \
    && mkdir -p /usr/opt \
    && cd /usr/opt \
    && git clone https://github.com/menduz/protobuf-wasm-lib

RUN cd /usr/opt/protobuf-wasm-lib \
    && /bin/bash -c "BRANCH=v3.6.1 ./prepare.sh"
RUN cd /usr/opt/protobuf-wasm-lib/protobuf \
    && bash -x ../build.sh
RUN cp /usr/bin/protoc /usr/opt/protobuf-wasm-lib/dist/bin/protoc

RUN apt-get --no-install-recommends -y install \
    libprotobuf-dev

# Necessary for benchmarking
RUN pip3 install sacrebleu
