# Using Bergamot Translator in JavaScript

All the instructions below are meant to run from the current directory.

## Using JS APIs

See [node-test.js](./node-test.js) for an annotated example of how to use the WASM module. Most of the code from it can also be used in a browser context.

Alternatively refer to the file `test_page/js/worker.js` that demonstrates how to use the bergamot translator in JavaScript via a `<script>` tag.

## Demo

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
