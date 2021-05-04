# Bergamot Translator

[![CircleCI badge](https://img.shields.io/circleci/project/github/mozilla/bergamot-translator/main.svg?label=CircleCI)](https://circleci.com/gh/mozilla/bergamot-translator/)

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions

### Build Natively
1. Clone the repository using these instructions:
    ```bash
    git clone https://github.com/mozilla/bergamot-translator
    cd bergamot-translator
    ```
2. Compile

    Create a folder where you want to build all the artifacts (`build-native` in this case) and compile in that folder
    ```bash
    mkdir build-native
    cd build-native
    cmake ../
    make -j
    ```

### Build WASM
#### Compiling for the first time

1. Download and Install Emscripten using following instructions
    * Get the latest sdk: `git clone https://github.com/emscripten-core/emsdk.git`
    * Enter the cloned directory: `cd emsdk`
    * Install the lastest sdk tools: `./emsdk install latest`
    * Activate the latest sdk tools: `./emsdk activate latest`
    * Activate path variables: `source ./emsdk_env.sh`

2. Clone the repository using these instructions:
    ```bash
    git clone https://github.com/mozilla/bergamot-translator
    cd bergamot-translator
    ```

3. Download files (only required if you want to perform inference using build artifacts)

    It packages the vocabulary files into wasm binary, which is required only if you want to perform inference.
    The compilation commands will preload these files in Emscripten’s virtual file system.

    If you want to package bergamot project specific files, please follow these instructions:
    ```bash
    git clone --depth 1 --branch main --single-branch https://github.com/mozilla-applied-ml/bergamot-models
    mkdir models
    cp -rf bergamot-models/prod/* models
    gunzip models/*/*
    find models \( -type f -name "model*" -or -type f -name "lex*" \) -delete
    ```

4. Compile
    1. Create a folder where you want to build all the artefacts (`build-wasm` in this case)
        ```bash
        mkdir build-wasm
        cd build-wasm
        ```

    2. Compile the artefacts
        * If you want to package files into wasm binary then execute following commands (Replace `FILES_TO_PACKAGE` with the
        directory containing all the files to be packaged)

            ```bash
            emcmake cmake -DCOMPILE_WASM=on -DPACKAGE_DIR=FILES_TO_PACKAGE ../
            emmake make -j
            ```
            e.g. If you want to package bergamot project specific files (downloaded using step 3 above) then
            replace `FILES_TO_PACKAGE` with `../models`

        * If you don't want to package any file into wasm binary then execute following commands:
            ```bash
            emcmake cmake -DCOMPILE_WASM=on ../
            emmake make -j
            ```

        The wasm artifacts (.js and .wasm files) will be available in the build directory ("build-wasm" in this case).

    3. Enable SIMD Wormhole via Wasm instantiation API in generated artifacts
        ```bash
        bash ../wasm/patch-artifacts-enable-wormhole.sh
        ```

#### Recompiling
As long as you don't update any submodule, just follow steps in `4.ii` and `4.iii` to recompile.\
If you update a submodule, execute following command before executing steps in `4.ii` and `4.iii` to recompile.
```bash
git submodule update --init --recursive
```


## How to use

### Using Native version

The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder.\
A short example of how to use the APIs is provided in `app/main.cpp` file.

### Using WASM version

Please follow the `README` inside the `wasm` folder of this repository that demonstrates how to use the translator in JavaScript.
