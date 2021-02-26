## Using Bergamot Translator in JavaScript
The example file `bergamot.html` in the folder `test_page` demonstrates how to use the bergamot translator in JavaScript via a `<script>` tag.

Please note that everything below assumes that the [bergamot project specific model files](https://github.com/mozilla-applied-ml/bergamot-models) were packaged in wasm binary (using the compile instructions given in the top level README).

### Using JS APIs

```js
// The model configuration as YAML formatted string. For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
// This example captures the most relevant options: model file, vocabulary files and shortlist file
const modelConfig = "{\"models\":[\"/esen/model.esen.npz\"],\"vocabs\":[\"/esen/vocab.esen.spm\",\"/esen/vocab.esen.spm\"],\"shortlist\":[\"/esen/lex.esen.s2t\"],\"beam-size\":1}";

// Instantiate the TranslationModel
const model = new Module.TranslationModel(modelConfig);

// Instantiate the arguments of translate() API i.e. TranslationRequest and input (vector<string>)
const request = new Module.TranslationRequest();
const input = new Module.VectorString;

// Initialize the input
input.push_back("Hola"); input.push_back("Mundo");

// translate the input; the result is a vector<TranslationResult>
const result = model.translate(input, request);

// Print original and translated text from each entry of vector<TranslationResult>
for (let i = 0; i < result.size(); i++) {
    console.log(' original=' + result.get(i).getOriginalText() + ', translation=' + result.get(i).getTranslatedText());
}

// Don't forget to clean up the instances
model.delete();
request.delete();
input.delete();
```

### Demo (see everything in action)

* Start the test webserver (ensure you have the latest nodejs installed)
    ```bash
    cd test_page
    bash start_server.sh
    ```
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
    http://localhost:8000/bergamot.html
    ```

* Run some translations:
    * Choose a model and press `Load Model`
    * Type a sentence to be translated in the `From` textbox and press `Translate`
    * See the results in the `To` and `Log` textboxes