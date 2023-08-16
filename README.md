# Bergamot Translator

[![CircleCI badge](https://img.shields.io/circleci/project/github/browsermt/bergamot-translator/main.svg?label=CircleCI)](https://circleci.com/gh/browsermt/bergamot-translator/)

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser. Read more about this project in the [developer documentation](https://browser.mt/docs/main/index.html).

## Build Instructions

### Build Natively

Create a folder where you want to build all the artifacts (`build-native` in this case) and compile

```bash
mkdir build-native
cd build-native
cmake ../
make -j2
```

For more detailed build instructions read the [Bergamot C++ Library](https://browser.mt/docs/main/marian-integration.html) docs.

### Build Wasm

The process for building Wasm is controlled by the `build-wasm.sh` script. This script downloads the emscripten toolchain and generates the build artifacts in the `build-wasm` folder.

```bash
./build-wasm.sh
```

For more information on running the Wasm see [Using Bergamot Translator in JavaScript](https://browser.mt/docs/main/wasm-example.html).

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
A short example of how to use the APIs is provided in `app/bergamot.cpp` file.

### Using Wasm version

Please follow the `README` inside the `wasm` folder of this repository that demonstrates how to use the translator in JavaScript.
