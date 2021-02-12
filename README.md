# Bergamot Translator

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions

### Build Natively

```bash
git clone  --recursive https://github.com/browsermt/bergamot-translator
cd bergamot-translator
mkdir build
cd build
cmake ../
make -j
```

### Build WASM

To compile WASM, first download and Install Emscripten using following instructions:

1. Get the latest sdk: `git clone https://github.com/emscripten-core/emsdk.git`
2. Enter the cloned directory: `cd emsdk`
3. Install the lastest sdk tools: `./emsdk install latest`
4. Activate the latest sdk tools: `./emsdk activate latest`
5. Activate path variables: `source ./emsdk_env.sh`

After the successful installation of Emscripten, perform these steps:

```bash
git clone https://github.com/browsermt/bergamot-translator
cd bergamot-translator
mkdir build-wasm
cd build-wasm
emcmake cmake -DCOMPILE_WASM=on ../
emmake make -j
```

It should generate the artefacts (.js and .wasm files) in `wasm` folder inside build directory ("build-wasm" in this case).

The build also allows packaging files into wasm binary (i.e. preloading in Emscripten’s virtual file system) using cmake
option `PACKAGE_DIR`. The compile command below packages all the files in PATH directory into wasm binary.
```bash
emcmake cmake -DCOMPILE_WASM=on -DPACKAGE_DIR=<PATH> ../
```
Files packaged this way are preloaded in the root of the virtual file system.


After Editing Files:

```bash
emmake make -j
```

After Adding/Removing Files:

```bash
emcmake cmake -DCOMPILE_WASM=on ../
emmake make -j
```

### Using Native version

The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder. A short example of how to use the APIs is provided in `app/main.cpp` file

### Using WASM version

Please follow the `README` inside the `wasm` folder of this repository that demonstrates how to use the translator in JavaScript.
