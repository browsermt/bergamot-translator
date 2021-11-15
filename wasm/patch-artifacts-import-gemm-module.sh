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
sed -i.bak 's/"env"[[:space:]]*:[[:space:]]*asmLibraryArg,/"env": asmLibraryArg,\
    "wasm_gemm":{\
    "int8_prepare_a": (...a) => Module["asm"].int8PrepareAFallback(...a),\
    "int8_prepare_b": (...a) => Module["asm"].int8PrepareBFallback(...a),\
    "int8_prepare_b_from_transposed": (...a) => Module["asm"].int8PrepareBFromTransposedFallback(...a),\
    "int8_prepare_b_from_quantized_transposed": (...a) => Module["asm"].int8PrepareBFromQuantizedTransposedFallback(...a),\
    "int8_prepare_bias": (...a) => Module["asm"].int8PrepareBiasFallback(...a),\
    "int8_multiply_and_add_bias": (...a) => Module["asm"].int8MultiplyAndAddBiasFallback(...a),\
    "int8_select_columns_of_b": (...a) => Module["asm"].int8SelectColumnsOfBFallback(...a),\
    },/g' ${ARTIFACT}
echo "SUCCESS"
