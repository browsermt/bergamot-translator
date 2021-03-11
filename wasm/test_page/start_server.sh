#!/bin/bash
echo "Start: Copying artifacts in local folder------"
cp ../../build-wasm/wasm/bergamot-translator-worker.data .
cp ../../build-wasm/wasm/bergamot-translator-worker.js .
cp ../../build-wasm/wasm/bergamot-translator-worker.wasm .
cp ../../build-wasm/wasm/bergamot-translator-worker.worker.js .
echo "Done----"

echo "Start: Enabling wormhole via APIs that compile and instantiate wasm module-------"
sed -i.bak 's/var result = WebAssembly.instantiateStreaming(response, info);/var result = WebAssembly.instantiateStreaming(response, info, {simdWormhole:true});/g' bergamot-translator-worker.js
sed -i.bak 's/return WebAssembly.instantiate(binary, info);/return WebAssembly.instantiate(binary, info, {simdWormhole:true});/g' bergamot-translator-worker.js
sed -i.bak 's/var module = new WebAssembly.Module(bytes);/var module = new WebAssembly.Module(bytes, {simdWormhole:true});/g' bergamot-translator-worker.js
echo "Done: Enabling wormhole via APIs that compile and instantiate wasm module--------"

npm install
echo "Start httpserver"
node bergamot-httpserver.js