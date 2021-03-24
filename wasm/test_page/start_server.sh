#!/bin/bash
echo "Start: Copying artifacts in local folder------"
cp ../../build-wasm/wasm/bergamot-translator-worker.data .
cp ../../build-wasm/wasm/bergamot-translator-worker.js .
cp ../../build-wasm/wasm/bergamot-translator-worker.wasm .
cp ../../build-wasm/wasm/bergamot-translator-worker.worker.js .

npm install
echo "Start httpserver"
node bergamot-httpserver.js