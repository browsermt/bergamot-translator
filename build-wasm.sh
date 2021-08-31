#!/usr/bin/env bash
set -e
set -x

# Usage
Usage="Build translator to wasm (with/without wormhole).

Usage: $(basename "$0") [WORMHOLE]

    where:
    WORMHOLE      An optional string argument
                  - when specified on command line, builds wasm artifacts with wormhole
                  - when not specified (the default behaviour), builds wasm artifacts without wormhole."

if [ "$#" -gt 1 ]; then
  echo "Illegal number of parameters passed"
  echo "$Usage"
  exit
fi

WORMHOLE=false

if [ "$#" -eq 1 ]; then
  if [ "$1" = "WORMHOLE" ]; then
    WORMHOLE=true
  else
    echo "Illegal parameter passed"
    echo "$Usage"
    exit
  fi
fi

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
    ./emsdk install 2.0.9
    ./emsdk activate 2.0.9
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

if [ "$WORMHOLE" = true ]; then
  emcmake cmake -DCOMPILE_WASM=on ../
else
  emcmake cmake -DCOMPILE_WASM=on -DWORMHOLE=off ../
fi
emmake make -j2

#     2. Enable SIMD Wormhole via Wasm instantiation API in generated artifacts
if [ "$WORMHOLE" = true ]; then
  bash ../wasm/patch-artifacts-enable-wormhole.sh
fi

# The artifacts (.js and .wasm files) will be available in the build directory
exit 0
