const {Blob} = require('buffer');
const fs = require('fs');
const https = require('https');
const {JSDOM} = require('jsdom');


const wasmBinary = fs.readFileSync('./bergamot-translator-worker.wasm');
global.Module = {
  wasmBinary,
  onRuntimeInitialized
};

// Execute bergamot-translation-worker.js in this scope
const js = fs.readFileSync('./bergamot-translator-worker.js', {encoding: 'utf8'});
eval.call(global, js);

/**
 * Helper to download file into ArrayBuffer.
 */
function download(url) {
  return new Promise((accept, reject) => {
    https.get(url, (res) => {
      const chunks = [];
      res.on('error', reject);
      res.on('data', chunk => chunks.push(chunk));
      res.on('end', async () => {
        const data = new Blob(chunks);
        data.arrayBuffer().then(accept, reject);
      });
    });
  });
}

/**
 * Loads ArrayBuffer into AlignedMemory.
 */
function load(buffer, alignment) {
  const bytes = new Int8Array(buffer);
  const memory = new Module.AlignedMemory(bytes.byteLength, alignment);
  memory.getByteArrayView().set(bytes);
  return memory;
}

/**
 * Called from inside the worker.js script once the wasm module is loaded
 * and all the emscripten magic and linking has been done.
 */
async function onRuntimeInitialized() {
  // Root url for our models for now.
  const root = 'https://storage.googleapis.com/bergamot-models-sandbox/0.2.14';

  // In order of TranslationMemory's arguments
  const files = [
    {url: `${root}/ende/model.ende.intgemm.alphas.bin`, alignment: 256},
    {url: `${root}/ende/lex.50.50.ende.s2t.bin`, alignment: 64},
    {url: `${root}/ende/vocab.deen.spm`, alignment: 64},
  ];

  // Download model data and load it into aligned memory
  const [modelMem, shortlistMem, vocabMem] = await Promise.all(files.map(async (file) => {
    return load(await download(file.url), file.alignment);
  }));

  // Config yaml (split as array to allow for indentation without adding tabs
  // or spaces to the strings themselves.)
  const config = [
    'beam-size: 1',
    'normalize: 1.0',
    'word-penalty: 0',
    'alignment: soft',
    'max-length-break: 128',
    'mini-batch-words: 1024',
    'workspace: 128',
    'max-length-factor: 2.0',
    'skip-cost: true',
    'cpu-threads: 0',
    'quiet: true',
    'quiet-translation: true',
    'gemm-precision: int8shiftAll',
  ].join('\n');

  // Set up translation service
  const service = new Module.BlockingService({cacheSize: 0});

  // Put vocab into its own std::vector<AlignedMemory>
  const vocabs = new Module.AlignedMemoryList();
  vocabs.push_back(vocabMem);

  // Setup up model with config yaml and AlignedMemory objects
  const model = new Module.TranslationModel(config, modelMem, shortlistMem, vocabs, /*qualityModel=*/ null);

  // Construct std::vector<std::string> inputs;
  const input = new Module.VectorString();
  input.push_back('<p> Hello world! </p> <p> Goodbye World! </p>');

  // Construct std::vector<ResponseOptions>
  const options = new Module.VectorResponseOptions();
  options.push_back({qualityScores: false, alignment: true, html: true});

  // Translate our batch (of 1)
  const output = service.translate(model, input, options);

  // Get output from std::vector<Response>
  // The following works as a simple black-box test of the API, based on
  // properties of HTML.
  const translation = output.get(0).getTranslatedText()

  // Print raw translation for inspection.
  console.log(translation)

  const fragment = JSDOM.fragment(translation)

  // Print two expected tags.
  console.log(fragment.firstElementChild.outerHTML)
  console.log(fragment.lastElementChild.outerHTML)

  // Assertion that there are two children at the output.
  assert(fragment.childElementCount === 1);


  // Clean-up
  input.delete();
  options.delete();
  output.delete();
}
