# Using Bergamot Translator in JavaScript

Instructions in this document assume current-directory to be
[wasm](https://github.com/browsermt/bergamot-translator/tree/main/wasm) within
bergamot-translator source.

The example file `bergamot.html` in the folder `test_page` demonstrates how to
use the bergamot translator in JavaScript via a `<script>` tag.

## Pre-requisites

**Download files required for translation**

Please note that [Using JS APIs](#using-js-apis) and [Demo](#demo) section below assumes that the [bergamot project specific model files](https://github.com/mozilla-applied-ml/bergamot-models) are already downloaded and present in the `test_page` folder. If this is not done then use following instructions to do so:

```bash
cd test_page
git clone --depth 1 --branch main --single-branch https://github.com/mozilla-applied-ml/bergamot-models
mkdir models
cp -rf bergamot-models/prod/* models
gunzip models/*/*
```

## Using JS APIs

```js
// The model configuration as YAML formatted string. For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
// This example captures some of the most relevant options
const modelConfig = `beam-size: 1
normalize: 1.0
word-penalty: 0
max-length-break: 128
mini-batch-words: 1024
workspace: 128
max-length-factor: 2.0
skip-cost: true
cpu-threads: 0
quiet: true
quiet-translation: true
gemm-precision: int8shift
`;

// Download model, shortlist and vocabulary files and read them into buffers
const modelFile = `models/esen/model.esen.intgemm.alphas.bin`;
const shortlistFile = `models/esen/lex.50.50.esen.s2t.bin`;
const vocabFiles = [`models/${languagePair}/vocab.${vocabLanguagePair}.spm`,
                    `models/${languagePair}/vocab.${vocabLanguagePair}.spm`];
const uniqueVocabFiles = new Set(vocabFiles);

// Please refer to bergamot.html in test_page folder for downloadAsArrayBuffer function
const downloadedBuffers = await Promise.all([downloadAsArrayBuffer(modelFile), downloadAsArrayBuffer(shortlistFile)]);
const downloadedVocabBuffers = [];
for (let item of uniqueVocabFiles.values()) {
  downloadedVocabBuffers.push(await downloadAsArrayBuffer(item));
}

const modelBuffer = downloadedBuffers[0];
const shortListBuffer = downloadedBuffers[1];

// Construct AlignedMemory instances from the buffers
var alignedModelMemory = constructAlignedMemoryFromBuffer(modelBuffer, 256); // Please refer to bergamot.html in test_page folder for this function
var alignedShortlistMemory = constructAlignedMemoryFromBuffer(shortListBuffer, 64); // Please refer to bergamot.html in test_page folder for this function
var alignedVocabsMemoryList = new Module.AlignedMemoryList;
downloadedVocabBuffers.forEach(item => alignedVocabsMemoryList.push_back(constructAlignedMemoryFromBuffer(item, 64)));

// Instantiate the Translation Service
const translationService = new Module.Service(modelConfig, alignedModelMemory, alignedShortlistMemory, alignedVocabsMemoryList);

// Instantiate the arguments of translate() API i.e. ResponseOptions and input (vector<string>)
const responseOptions = new Module.ResponseOptions();
const input = new Module.VectorString;

// Initialize the input
input.push_back("Hola"); input.push_back("Mundo");

// translate the input; the result is a vector<Response>
const result = translationService.translate(input, responseOptions);

// Print original and translated text from each entry of vector<Response>
for (let i = 0; i < result.size(); i++) {
    console.log(' original=' + result.get(i).getOriginalText() + ', translation=' + result.get(i).getTranslatedText());
}

// Don't forget to clean up the instances
translationService.delete();
responseOptions.delete();
input.delete();
```

## Demo 

* Make sure that you followed [Pre-requisites](#pre-requisites) instructions before moving forward.

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
    http://localhost:8000/bergamot.html
    ```

* Run some translations:
    * Choose a model and press `Load Model`
    * Type a sentence to be translated in the `From` textbox and press `Translate`
    * See the results in the `To` and `Log` textboxes
