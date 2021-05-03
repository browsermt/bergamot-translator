#!/bin/bash

echo "Patching wasm artifacts to enable wormhole via APIs that compile and instantiate wasm module"
sed -i.bak 's/WebAssembly.instantiateStreaming[[:space:]]*([[:space:]]*response[[:space:]]*,[[:space:]]*info[[:space:]]*)/WebAssembly.instantiateStreaming(response, info, {simdWormhole:true})/g' wasm/bergamot-translator-worker.js
sed -i.bak 's/WebAssembly.instantiate[[:space:]]*([[:space:]]*binary[[:space:]]*,[[:space:]]*info[[:space:]]*)/WebAssembly.instantiate(binary, info, {simdWormhole:true})/g' wasm/bergamot-translator-worker.js
sed -i.bak 's/WebAssembly.Module[[:space:]]*([[:space:]]*bytes[[:space:]]*)/WebAssembly.Module(bytes, {simdWormhole:true})/g' wasm/bergamot-translator-worker.js
echo "Done"
