#!/usr/bin/env bash

# Usage: ./build-wasm.sh

set -e
set -x

# Run script from the context of the script-containing directory
cd "$(dirname $0)"

# This file replicates the instructions found in ./README.md under "Build WASM"

# Prerequisite: Download and Install Emscripten using following instructions (unless the EMSDK env var is already set)
if [ "$EMSDK" == "" ]; then
  EMSDK_UPDATE_REQUIRED=0
  if [ ! -d "emsdk" ]; then
    git clone https://github.com/emscripten-core/emsdk.git
    EMSDK_UPDATE_REQUIRED=1
  else
    cd emsdk
    git fetch
    # Only pull if necessary
    if [ $(git rev-parse HEAD) != $(git rev-parse @{u}) ]; then
      git pull --ff-only
      EMSDK_UPDATE_REQUIRED=1
    fi
    cd -
  fi
  if [ "$EMSDK_UPDATE_REQUIRED" == "1" ]; then
    cd emsdk
    ./emsdk install 2.0.9
    ./emsdk activate 2.0.9
    cd -
  fi
  source ./emsdk/emsdk_env.sh
fi

# Compile
#    1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
if [ ! -d "build-wasm" ]; then
  mkdir build-wasm
fi
cd build-wasm
emcmake cmake -DCOMPILE_WASM=on ../
emmake make -j2

#     2. Enable SIMD Wormhole via Wasm instantiation API in generated artifacts
bash ../wasm/patch-artifacts-enable-wormhole.sh

# The artifacts (.js and .wasm files) will be available in the build directory ("build-wasm" in this case).

exit 0
