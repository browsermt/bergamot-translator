## Using Bergamot Translator in JavaScript
The example file `bergamot.html` in this folder demonstrates how to use the bergamot translator in JavaScript via a `<script>` tag.

A brief summary is here though:
```
// Instantiate the TranslationModelConfiguration
var modelConfig = new Module.TranslationModelConfiguration("dummy_modelFilePath","dummy_sourceVocabPath","dummy_targetVocabPath");

// Instantiate the TranslationModel
var model = new Module.TranslationModel(modelConfig);

// Instantiate the arguments of translate() API i.e. TranslationRequest and input (vector<string>)
var request = new Module.TranslationRequest();
let input = new Module.VectorString;

// Initialize the input
input.push_back("Hola"); input.push_back("Mundo");

// translate the input; the result is a vector<TranslationResult>
var result = model.translate(input, request);

// Print original and translated text from each entry of vector<TranslationResult>
for (let i = 0; i < result.size(); i++) {
    console.log(' original=' + result.get(i).getOriginalText() + ', translation=' + result.get(i).getTranslatedText());
}

// Clean up the instances
modelConfig.delete();
model.delete();
request.delete();
input.delete();
```

You can also see everything in action by running `bergamot.html` file in the browser following these steps:
* Copy bergamot.html to the directory where build artefacts of wasm compilation (i.e. .js and .wasm files) reside.
* Start an http server locally
* Open the link provided by the http server in any browser
* Open the browser's console and you will see all the console messages there

Assuming build artefacts are present in `$ROOT/build-wasm` folder where `ROOT` is repository's root.
Above instructions would become:
```
$ cd $ROOT/build-wasm
$ cp ../wasm/bergamot.html wasm/.
$ python3 -m http.server -d wasm

Assuming it starts the http server on 8000 port,
open `http://0.0.0.0:8000/bergamot.html` in any browser and see the console logs in browser's console.
```