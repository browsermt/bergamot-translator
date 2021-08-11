# Bergamot Translator

[![CircleCI badge](https://img.shields.io/circleci/project/github/mozilla/bergamot-translator/main.svg?label=CircleCI)](https://circleci.com/gh/mozilla/bergamot-translator/)

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions

### Build Natively
Create a folder where you want to build all the artifacts (`build-native` in this case) and compile

```bash
mkdir build-native
cd build-native
cmake ../
make -j2
```

### Build WASM
#### Prerequisite

Building on wasm requires Emscripten toolchain. It can be downloaded and installed using following instructions:

* Get the latest sdk: `git clone https://github.com/emscripten-core/emsdk.git`
* Enter the cloned directory: `cd emsdk`
* Install the lastest sdk tools: `./emsdk install 2.0.9`
* Activate the latest sdk tools: `./emsdk activate 2.0.9`
* Activate path variables: `source ./emsdk_env.sh`

#### <a name="Compile"></a> Compile

To build a version that translates with higher speeds on Firefox Nightly browser, follow these instructions:

   1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
       ```bash
       mkdir build-wasm
       cd build-wasm
       emcmake cmake -DCOMPILE_WASM=on ../
       emmake make -j2
       ```

       The wasm artifacts (.js and .wasm files) will be available in the build directory ("build-wasm" in this case).

   2. Enable SIMD Wormhole via Wasm instantiation API in generated artifacts
       ```bash
       bash ../wasm/patch-artifacts-enable-wormhole.sh
       ```

To build a version that runs on all browsers (including Firefox Nightly) but translates slowly, follow these instructions:

  1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
      ```bash
      mkdir build-wasm
      cd build-wasm
      emcmake cmake -DCOMPILE_WASM=on -DWORMHOLE=off ../
      emmake make -j2
      ```

#### Recompiling
As long as you don't update any submodule, just follow [Compile](#Compile) steps.\
If you update a submodule, execute following command in repository root folder before executing
[Compile](#Compile) steps.
```bash
git submodule update --init --recursive
```


## How to use

### Using Native version

The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder.\
A short example of how to use the APIs is provided in `app/main.cpp` file.

### Using WASM version

Please follow the `README` inside the `wasm` folder of this repository that demonstrates how to use the translator in JavaScript.
