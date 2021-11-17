#!/bin/bash
usage="Patch wasm artifacts to import gemm implementation for wasm.

Usage: $(basename "$0") [ARTIFACTS_FOLDER]

    where:
    ARTIFACTS_FOLDER    Folder containing wasm artifacts
                             (An optional argument, if unspecified the default is: current folder)"

if [ "$#" -gt 1 ]; then
    echo "Illegal number of parameters passed"
    echo "$usage"
    exit
fi

# Parse wasm artifacts folder if provided via script argument or set it to default
ARTIFACTS_FOLDER=$PWD
if [ "$#" -eq 1 ]; then
    if [ ! -e "$1" ]; then
        echo "Error: Folder \""$1"\" doesn't exist"
        exit
    fi
    ARTIFACTS_FOLDER="$1"
fi

ARTIFACT="$ARTIFACTS_FOLDER/bergamot-translator-worker.js"
if [ ! -e "$ARTIFACT" ]; then
    echo "Error: Artifact \"$ARTIFACT\" doesn't exist"
    exit
fi

echo "Importing integer (8-bit) gemm implementation"
SCRIPT_ABSOLUTE_PATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
sed -i.bak 's/"env"[[:space:]]*:[[:space:]]*asmLibraryArg,/"env": asmLibraryArg,\
    "wasm_gemm": createWasmGemm(),/g' ${ARTIFACT}
cat $SCRIPT_ABSOLUTE_PATH/import-gemm-module.js >> ${ARTIFACT}
echo "SUCCESS"
