#!/bin/bash

cp ../../build-wasm-docker/wasm/bergamot-translator-worker.data .
cp ../../build-wasm-docker/wasm/bergamot-translator-worker.js .
cp ../../build-wasm-docker/wasm/bergamot-translator-worker.wasm .
cp ../../build-wasm-docker/wasm/bergamot-translator-worker.worker.js .
npm install
node bergamot-httpserver.js