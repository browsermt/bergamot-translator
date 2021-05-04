#!/bin/bash
usage="Patch wasm artifacts to enable wormhole via APIs that compile and instantiate wasm module.

Usage: $(basename "$0") [WASM_ARTIFACTS_FOLDER]

    where:
    WASM_ARTIFACTS_FOLDER    Folder containing wasm artifacts
                             (An optional argument, if unspecified the default is: current folder)"

if [ "$#" -gt 1 ]; then
    echo "Illegal number of parameters passed"
    echo "$usage"
    exit
fi

# Parse wasm artifacts folder if provided via script argument or set it to default
WASM_ARTIFACTS_FOLDER=$PWD
if [ "$#" -eq 1 ]; then
    if [ ! -e "$1" ]; then
        echo "Error: Folder \""$1"\" doesn't exist"
        exit
    fi
    WASM_ARTIFACTS_FOLDER="$1"
fi

WASM_ARTIFACTS="$WASM_ARTIFACTS_FOLDER/bergamot-translator-worker.js"
if [ ! -e "$WASM_ARTIFACTS" ]; then
    echo "Error: Artifact \"$WASM_ARTIFACTS\" doesn't exist"
    exit
fi

echo "Patching \"$WASM_ARTIFACTS\" to enable wormhole via APIs that compile and instantiate wasm module"
sed -i.bak 's/WebAssembly.instantiateStreaming[[:space:]]*([[:space:]]*response[[:space:]]*,[[:space:]]*info[[:space:]]*)/WebAssembly.instantiateStreaming(response, info, {simdWormhole:true})/g' $WASM_ARTIFACTS
sed -i.bak 's/WebAssembly.instantiate[[:space:]]*([[:space:]]*binary[[:space:]]*,[[:space:]]*info[[:space:]]*)/WebAssembly.instantiate(binary, info, {simdWormhole:true})/g' $WASM_ARTIFACTS
sed -i.bak 's/WebAssembly.Module[[:space:]]*([[:space:]]*bytes[[:space:]]*)/WebAssembly.Module(bytes, {simdWormhole:true})/g' $WASM_ARTIFACTS
echo "Done"
