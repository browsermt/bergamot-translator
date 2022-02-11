# Javascript and WebAssembly

## Build WebAssembly

### Prerequisites

Building on wasm requires Emscripten toolchain. It can be downloaded and
installed using following instructions:

* Get the latest sdk: `git clone https://github.com/emscripten-core/emsdk.git`
* Enter the cloned directory: `cd emsdk`
* Install the lastest sdk tools: `./emsdk install 2.0.9`
* Activate the latest sdk tools: `./emsdk activate 2.0.9`
* Activate path variables: `source ./emsdk_env.sh`

### Compile

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

   3. Patch generated artifacts to import GEMM library from a separate wasm module
       ```bash
       bash ../wasm/patch-artifacts-import-gemm-module.sh
       ```

To build a version that runs on all browsers (including Firefox Nightly) but translates slowly, follow these instructions:

  1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
      ```bash
      mkdir build-wasm
      cd build-wasm
      emcmake cmake -DCOMPILE_WASM=on -DWORMHOLE=off ../
      emmake make -j2
      ```

  2. Patch generated artifacts to import GEMM library from a separate wasm module
       ```bash
       bash ../wasm/patch-artifacts-import-gemm-module.sh
       ```

### Recompiling

As long as you don't update any submodule, just follow [Compile](#Compile) steps.\
If you update a submodule, execute following command in repository root folder before executing
[Compile](#Compile) steps.
```bash
git submodule update --init --recursive
```

## Using in JavaScript

### Using JS APIs

Please refer to the file `test_page/js/worker.js` that demonstrates how to use the bergamot translator in JavaScript via a `<script>` tag.

### Demo

* Download bergamot model files required for translation

    Use following instructions to download [model files](https://github.com/mozilla/firefox-translations-models/) (make sure that `git-lfs` is installed and initialized before running these instructions):

    ```bash
    cd test_page
    git clone --depth 1 --branch main --single-branch https://github.com/mozilla/firefox-translations-models/
    mkdir models
    cp -rf firefox-translations-models/registry.json models
    cp -rf firefox-translations-models/models/prod/* models
    cp -rf firefox-translations-models/models/dev/* models
    gunzip models/*/*
    ```

* Start the test webserver (ensure you have the latest nodejs installed)
    ```bash
    cd test_page
    bash start_server.sh ../../build-wasm
    ```

    Provide the folder containing the wasm artifacts as the first argument of `start_server.sh` script (`../../build-wasm` in this case).

* Open any of the browsers below
    * Firefox Nightly +87: make sure the following prefs are on (about:config)
        ```
        dom.postMessage.sharedArrayBuffer.bypassCOOP_COEP.insecure.enabled = true
        javascript.options.wasm_simd = true
        javascript.options.wasm_simd_wormhole = true
        ```

    * Chrome Canary +90: start with the following argument
        ```
        --js-flags="--experimental-wasm-simd"
        ```

* Browse to the following page:
    ```
    http://localhost:80
    ```

* Perform translations:
    * Choose the source and target languages using `From` and `To` dropdowns.
    * Type a sentence to be translated in the `From` textbox.
    * See the result in the `To` textbox.
