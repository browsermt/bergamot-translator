# Installation

```bash
npm install @browsermt/bergamot-translator
```

# Quick start

```js
import {BatchTranslator} from "@browsermt/bergamot-translator/translator.js";

const translator = new BatchTranslator();

const response = await translator.translate({
  from: "en",
  to: "es",
  text: "Hello <em>world</em>!",
  html: true
});

console.log(response.target.text);

// Stops worker threads
translator.delete();
```

# Throughput vs Latency

This package comes with two translator implementations:

- [LatencyOptimisedTranslator](#latencyoptimisedtranslator) is more useful for an interactive session, say like Google Translate, where you're only working on translating one input at a time.
- [BatchTranslator](#batchtranslator) is optimised for processing a large number of translations as fast as possible (but individual translations might take some time), e.g. translating a large number of strings or all paragraphs in a document.

## LantencyOptimisedTranslator

Translator best suited for interactive usage. Runs with a single worker thread and a batch-size of 1 to give you a response as quickly as possible. It will cancel any pending translations that aren't currently being processed if you submit a new one.

```js
const translator = new LatencyOptimisedTranslator({
  pivotLanguage?: string?,
  registryUrl?: string,
  workerUrl?: string,
  downloadTimeout?: number,
  cacheSize?: number,
  useNativeIntGemm?: boolean,
})
```

- `pivotLanguage` - language code for the language to use as an intermediate if there is no direct translation model available. Defaults to `"en"`. Set to `null` to disable pivoting.
- `registryUrl` - url to a list of models and their paths. Defaults to `https://storage.googleapis.com/bergamot-models-sandbox/0.3.3/registry.json`.
- `workerUrl` - url to `translator-worker.js`. Defaults to `"worker/translator-worker.js"` relative to the path of `translator.js`.
- `downloadTimeout` - Maximum time we're attempting to download model files before failing. Defaults to `60000` or 60 seconds. Set to `0` to disable.
- `cacheSize` - Maximum number of sentences in kept translation cache (per worker, workers do not share their cache). This is an ideal maximum as it is a hash-map, in practice about 1/3th is occupied. If set to `0`, translation cache is disabled (the default).
- `useNativeIntGemm` - Try to link to native IntGEMM implementation when loading the WASM binary. This is only implemented in the privileged extension context of Firefox Nightly. If it fails, it will always fall back to the included implementation. Defaults to `false`.

### translate()

```js
const {request, target: {text:string}} = await translator.translate({
  from: string,
  to: string,
  text: string,
  html?: boolean,
  qualityScores?: boolean
})
```

Submits a translation request. Multiple of these are processed in a batch. A batch will be started the next tick (if there is a worker available).

- `from` - language code of the source language, e.g. `"de"`
- `to` - language code of the target language, e.g. `"en"`
- `text` - string of text to translate, e.g. `"Hallo Welt!"`
- `html` - boolean indicating whether `text` contains just plain text or HTML
- `qualityScores` - whether to calculate quality scores. Not all models support this, and you need to load a separate quality scores model file for it. Quality scores are returned as `<font x-bergamot-sentence-quality="">` and `<font x-bergamot-word-quality="">` wrapped around sentences and words in the output. When enabled, the output is always HTML, regardless of whether the input was.

Returns:

A promise to a translation response object, with `target.text` being the text or HTML of the translated output, and `request` a reference to the original translation request.

### delete()

```js
translator.delete()
```

Cancels all pending requests with a `CancelledError` and terminates the worker immediately. This will free all the resources used.

In a nodejs context you'll need to call this, otherwise your script won't exit because the translator will still be listening for messages from the worker.

## BatchTranslator

```js
const translator = new BatchTranslator({
  pivotLanguage?: string?,
  registryUrl?: string,
  workerUrl?: string,
  downloadTimeout?: number,
  cacheSize?: number,
  useNativeIntGemm?: boolean,
  workers?: number,
  batchSize?: number,
})
```

General translator options:

See [LatencyOptimisedTranslator](#latencyoptimisedtranslator).

BatchTranslator-specific options:

- `workers` - Number of worker threads. These are full-on instances of the translator, with their own copy of the model loaded. This is an upper bound. If not that many workers can be fed, it won't create new ones. Minimally 1. Default is `1`.
- `batchSize` - Number of translation requests per batch. All sentences from all translation requests are packed into a bunch of matrix operations. With a larger batch size the translator has more material to find ideal sets of sentences for filling the matrix. However, you'll only get the results for each of the requests in a batch once the whole batch is finished. Defaults to 8.

### translate()

```js
const {target: {text:string}} = await translator.translate({
  from: string,
  to: string,
  text: string,
  html?: boolean,
  qualityScores?: boolean,
  priority?: number
})
```

Submits a translation request. Multiple of these are processed in a batch. A batch will be started the next tick (if there is a worker available).

- (See [LatencyOptimisedTranslator.translate()](#translate) for most options)
- `priority` - When grouping translation requests into batches to give to workers, requests with a lower number are considered first. For example, if you're translating a web page, you can give requests of parts that are in the current frame a lower number to make sure they're processed first.

### remove()

```js
translator.remove(request => {
  // true deletes the request from the queue.
  return true;
})
```

Removes requests from the translation queue, i.e. only when they haven't been sent to a worker yet.

The filter function should return true-ish for each request that should be cancelled. Their promises are rejected with a `CancelledError` error.


### delete()

```js
translator.delete()
```

Cancels all pending requests with a `CancelledError` and terminates all workers immediately. This will free all the resources used.


# Models

Both translators accept a `backing` option, which tells it where to get model data and the translation engine implementation from. They default to using `BergamotTranslator` which gets its models from the same repository as [firefox-translations](https://github.com/mozilla/firefox-translations).

To customize the model, reimplement the `loadModelRegistry` and `loadTranslationModel` methods.

`loadModelRegistry()` has the hard requirement to return a promise to a list that looks like `{from: string, to: string, ...}[]`. The `from` and `to` keys are used as key for model selection.

`loadTranslationModel()` should return a promise with ArrayBuffers for `model`, `shortlist`, `vocabs`, and optionally `qualityModel`. It can include a `config` object as well.

Example of an alternative implementation that loads models from data.statmt.org, i.e. the same as [translateLocally](https://translateLocally.com):

```js
class CustomBacking extends TranslatorBacking {
    async loadModelRegistery() {
        const response = await fetch('https://translatelocally.com/models.json');
        const {models} = await response.json();

        // Add 'from' and 'to' keys for each model. Since theoretically a model
        // can have multiple froms keys in TranslateLocally, we do a little
        // product here.
        return models.reduce((list, model) => {
            try {
                const to = first(Intl.getCanonicalLocales(model.trgTag));
                for (let from of Intl.getCanonicalLocales(Object.keys(model.srcTags))) {
                    list.push({from, to, model});
                }
            } catch (err) {
                console.log('Skipped model', model, 'because', err);
            }

            return list;
        }, []);
    }

    async loadTranslationModel({from, to}) {
        // Find that model in the registry which will tell us about its files
        const entries = (await this.registry).filter(model => model.from === from && model.to === to);

        // Prefer tiny models above non-tiny ones
        entries.sort(({model: a}, {model: b}) => (a.shortName.indexOf('tiny') === -1 ? 1 : 0) - (b.shortName.indexOf('tiny') === -1 ? 1 : 0));

        if (!entries)
            throw new Error(`No model for '${from}' -> '${to}'`);

        const entry = entries[0].model;

        const response = await fetch(entry.url, {
            integrity: `sha256-${entry.checksum}`
        });

        // pako from https://www.npmjs.com/package/pako
        const archive = pako.inflate(await response.arrayBuffer());

        // untar from https://www.npmjs.com/package/js-untar
        const files = await untar(archive.buffer);

        const find = (filename) => {
            const found = files.find(file => file.name.match(/(?:^|\/)([^\/]+)$/)[1] === filename)
            if (found === undefined)
                throw new Error(`Could not find '${filename}' in model archive`);
            return found;
        };

        // YAML.parse is found in worker/translator-worker.js
        const config = YAML.parse(find('config.intgemm8bitalpha.yml').readAsString());

        const model = find(config.models[0]).buffer;

        const vocabs = config.vocabs.map(vocab => find(vocab).buffer);

        const shortlist = find(config.shortlist[0]).buffer;

        // Return the buffers
        return {model, vocabs, shortlist, config};
    }
}

const translator = new BatchTranslator(options, new CustomBacking(options));
```

# Supported languages

See https://github.com/mozilla/firefox-translations-models#currently-supported-languages. You may need to set the `registryUrl` option to point to the latest release.