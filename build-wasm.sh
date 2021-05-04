#!/usr/bin/env bash

# Usage: ./build-wasm.sh

set -e
set -x

# Run script from the context of the script-containing directory
cd "$(dirname $0)"

# This file replicates the instructions found in ./README.md under "Build WASM"
# with slight adjustments to be able to run the build script multiple times without having to clone all dependencies
# as per "As long as you don't update any submodule, just follow steps in `4.ii` to recompile."

# 1. Download and Install Emscripten using following instructions (unless the EMSDK env var is already set)
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
    ./emsdk install latest
    ./emsdk activate latest
    cd -
  fi
  source ./emsdk/emsdk_env.sh
fi

# 3. Download models (required to perform inference using build artifacts)
if [ ! -d "bergamot-models" ]; then
  git clone --depth 1 --branch main --single-branch https://github.com/mozilla-applied-ml/bergamot-models
else
  cd bergamot-models
  git fetch
  # Only pull if necessary
  if [ $(git rev-parse HEAD) != $(git rev-parse @{u}) ]; then
    git pull --ff-only
  fi
  cd -
fi
mkdir -p models
rm -rf models/*
cp -rf bergamot-models/prod/* models
gunzip models/*/*
find models \( -type f -name "model*" -or -type f -name "lex*" \) -delete

# 4. Compile
#     1. Create a folder where you want to build all the artifacts (`build-wasm` in this case)
if [ ! -d "build-wasm" ]; then
  mkdir build-wasm
fi
cd build-wasm

#     2. Compile the artifacts
emcmake cmake -DCOMPILE_WASM=on -DPACKAGE_DIR="../models/" ../
emmake make -j3

#     3. Enable SIMD Wormhole via Wasm instantiation API in generated artifacts
bash ../wasm/patch-artifacts-enable-wormhole.sh

# The artifacts (.js and .wasm files) will be available in the build directory ("build-wasm" in this case).

exit 0
