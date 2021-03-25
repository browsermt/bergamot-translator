#!/bin/bash

echo "Patching wasm artifacts to enable wormhole via APIs that compile and instantiate wasm module"
sed -i.bak 's/var result = WebAssembly.instantiateStreaming(response, info);/var result = WebAssembly.instantiateStreaming(response, info, {simdWormhole:true});/g' wasm/bergamot-translator-worker.js
sed -i.bak 's/return WebAssembly.instantiate(binary, info);/return WebAssembly.instantiate(binary, info, {simdWormhole:true});/g' wasm/bergamot-translator-worker.js
sed -i.bak 's/var module = new WebAssembly.Module(bytes);/var module = new WebAssembly.Module(bytes, {simdWormhole:true});/g' wasm/bergamot-translator-worker.js
echo "Done"
