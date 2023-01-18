#!/bin/bash

usage="Copy wasm artifacts from the given folder and start httpserver

Usage: $(basename "$0") [ARTIFACTS_SOURCE_FOLDER]

    where:
    ARTIFACTS_SOURCE_FOLDER    Directory containing pre-built wasm artifacts"

SCRIPT_ABSOLUTE_PATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters passed"
    echo "$usage"
    exit
fi

# Check if ARTIFACTS_SOURCE_FOLDER is valid or not
if [ ! -e "$1" ]; then
    echo "Error: Folder \""$1"\" doesn't exist"
    exit
fi

# Prepare a list all wasm artifacts to be copied and copy them to the destination folder
ARTIFACTS_BASE_NAME="bergamot-translator-worker"
ARTIFACTS="$1/$ARTIFACTS_BASE_NAME.js $1/$ARTIFACTS_BASE_NAME.wasm"
ARTIFACTS_DESTINATION_FOLDER=$SCRIPT_ABSOLUTE_PATH/../module/worker

for i in $ARTIFACTS; do
    [ -f "$i" ] || breaks
    cp $i $ARTIFACTS_DESTINATION_FOLDER
    echo "Copied \"$i\" to \"$ARTIFACTS_DESTINATION_FOLDER\""
done

# Start http server
(cd $SCRIPT_ABSOLUTE_PATH;
npm install;
echo "Start httpserver";
node bergamot-httpserver.js 80 1 0)
