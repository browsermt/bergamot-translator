#!/usr/bin/env bash
set -e
set -x

# Run script from the context of the script-containing directory
cd "$(dirname $0)"

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
    ./emsdk install 3.1.8
    ./emsdk activate 3.1.8
    cd -
  fi
  source ./emsdk/emsdk_env.sh
fi

# Compile
#    1. Create a folder where you want to build all the artifacts and compile
BUILD_DIRECTORY="build-wasm"
if [ ! -d ${BUILD_DIRECTORY} ]; then
  mkdir ${BUILD_DIRECTORY}
fi
cd ${BUILD_DIRECTORY}

emcmake cmake -DCOMPILE_WASM=on ../
emmake make -j2

#     2. Import GEMM library from a separate Wasm module
bash ../wasm/patch-artifacts-import-gemm-module.sh

set +x
echo ""
echo "Build complete"
echo ""
echo "  ./build-wasm/bergamot-translator-worker.js"
echo "  ./build-wasm/bergamot-translator-worker.wasm"

WASM_SIZE=$(wc -c bergamot-translator-worker.wasm | awk '{print $1}')
GZIP_SIZE=$(gzip -c bergamot-translator-worker.wasm | wc -c | xargs) # xargs trims the whitespace

# Convert it to human readable.
WASM_SIZE="$(awk 'BEGIN {printf "%.2f",'$WASM_SIZE'/1048576}')M ($WASM_SIZE bytes)"
GZIP_SIZE="$(awk 'BEGIN {printf "%.2f",'$GZIP_SIZE'/1048576}')M ($GZIP_SIZE bytes)"

echo "  Uncompressed wasm size: $WASM_SIZE"
echo "  Compressed wasm size: $GZIP_SIZE"

# The artifacts (.js and .wasm files) will be available in the build directory
exit 0
