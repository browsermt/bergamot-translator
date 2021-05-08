#!/bin/bash

usage="Copy wasm artifacts from build directory and start httpserver

Usage: $(basename "$0") [WASM_ARTIFACTS_FOLDER]

    where:
    WASM_ARTIFACTS_FOLDER    Folder containing pre-built wasm artifacts"

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters passed"
    echo "$usage"
    exit
fi

# Check if WASM_ARTIFACTS_FOLDER is valid or not
if [ ! -e "$1" ]; then
    echo "Error: Folder \""$1"\" doesn't exist"
    exit
fi

WASM_ARTIFACTS="$1/bergamot-translator-worker.*"
for i in $WASM_ARTIFACTS; do
    [ -f "$i" ] || breaks
    cp $i .
    echo "Copied \"$i\""
done

npm install
echo "Start httpserver"
node bergamot-httpserver.js