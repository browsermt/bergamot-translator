#!/usr/bin/env node
const fs = require('fs');

// Read wasm binary into a blob, which will be loaded by
// bergamot-translator-worker.js in a minute.
const wasmBinary = fs.readFileSync('./bergamot-translator-worker.wasm');

// Initialise the `Module` object for bergamot-translation-worker.js`. See that
// generated file, and the general Emscripten documentation for what kinds of
// properties can be defined here, and how they affect execution.
global.Module = {
  wasmBinary,
  onRuntimeInitialized
};

// Execute bergamot-translation-worker.js in this scope
const js = fs.readFileSync('./bergamot-translator-worker.js', {encoding: 'utf8'});
eval.call(global, js);

// At this point, `Module` will contain all the classes that are exported by
// the WASM binary.

/**
 * Helper to download file into ArrayBuffer.
 * @arg {String} url to binary blob
 * @return {Promise<ArrayBuffer>}
 */
async function download(url) {
  const response = await fetch(url);
  const blob = await response.blob();
  return blob.arrayBuffer();
}

/**
 * Loads ArrayBuffer into AlignedMemory. Bergamot translator uses instructions
 * that benefit from aligned memory for speed. This means we have to carefully
 * allocate blobs of memory inside the WASM vm that match that alignment
 * requirement, and then copy the binary data into that allocated blob.
 * @arg {ArrayBuffer} buffer with raw model data
 * @arg {Number} alignment (multiple of 64)
 * @return {Module.AlignedMemory}
 */
function load(buffer, alignment) {
  const bytes = new Int8Array(buffer);
  const memory = new Module.AlignedMemory(bytes.byteLength, alignment);
  memory.getByteArrayView().set(bytes);
  return memory;
}

/**
 * Called from inside the bergamot-translation-worker.js script once the wasm
 * module is loaded and all the Emscripten magic and linking has been done.
 */
async function onRuntimeInitialized() {
  // Root url for our models for now.
  const root = 'https://storage.googleapis.com/bergamot-models-sandbox/0.3.1';

  // Urls of all data files necessary to create a translation model for
  // English -> German. Note: list is in order of TranslationMemory's arguments.
  const files = [
    {url: `${root}/ende/model.ende.intgemm.alphas.bin`, alignment: 256},
    {url: `${root}/ende/lex.50.50.ende.s2t.bin`, alignment: 64},
    {url: `${root}/ende/vocab.deen.spm`, alignment: 64}, // "deen" may look the wrong way around but vocab is the same between de->en and en->de models.
  ];

  // Download model data and load it into aligned memory. AlignedMemory is a
  // necessary wrapper around allocated memory inside the WASM vm. This is
  // allocated at specific offsets, which is necessary for the performance of
  // some of the instructions inside bergamot-translator.
  const [modelMem, shortlistMem, vocabMem] = await Promise.all(files.map(async (file) => {
    return load(await download(file.url), file.alignment);
  }));

  // Config yaml (split as array to allow for indentation without adding tabs
  // or spaces to the strings themselves.)
  // See https://marian-nmt.github.io/docs/cmd/marian-decoder/ for the meaning
  // of most of these options and what other options might be available.
  const config = [
    'beam-size: 1',
    'normalize: 1.0',
    'word-penalty: 0',
    'alignment: soft', // is necessary if you want to use HTML at any point
    'max-length-break: 128',
    'mini-batch-words: 1024',
    'workspace: 128',
    'max-length-factor: 2.0',
    'skip-cost: true',
    'gemm-precision: int8shiftAll', // is necessary for speed and compatibility with Mozilla's models.
  ].join('\n');

  // Set up translation service. This service translates a batch of text per
  // call. The larger the batch, the faster the translation (in words per
  // second) happens, but the longer you have to wait for all of them to finish.
  // The constructor expects an object with options, but only one option is
  // currently supported: `cacheSize`. Setting this to `0` disables the
  // translation cache.
  // **Note**: cacheSize is the theoretical maximum number of sentences that
  // will be cached. In practise, about 1/3 of that will actually be used.
  // See https://github.com/XapaJIaMnu/translateLocally/pull/75
  const service = new Module.BlockingService({cacheSize: 0});

  // Put vocab into its own std::vector<AlignedMemory>. Most models for the
  // Bergamot project only have one vocabulary that is shared by both the input
  // and output side of the translator. But in theory, you could have one for
  // the input side and a different one for the output side. Hence: a list.
  const vocabs = new Module.AlignedMemoryList();
  vocabs.push_back(vocabMem);

  // Setup up model with config yaml and AlignedMemory objects. Optionally a
  // quality estimation model can also be loaded but this is not demonstrated
  // here. Generally you don't need it.
  const model = new Module.TranslationModel(config, modelMem, shortlistMem, vocabs, /*qualityModel=*/ null);

  // Construct std::vector<std::string> inputs; This is our batch!
  const input = new Module.VectorString();
  input.push_back('<p>Hello world! Let us write a second sentence.</p> <p>Goodbye World!</p>');

  // Construct std::vector<ResponseOptions>, one entry per input. Note that
  // all these three options need to be specified for each entry:
  // `qualityScores`: do we want to compute quality scores for the translation
  //   of each word? If true, each word will be wrapped in a <font> tag with
  //   a quality score attribute. Requires the qualityModel to be passed in
  //   when the `TranslationModel` is constructed. Slows down translation.
  // `alignment`: computes alignment scores that maps parts of the input text
  //   to parts of the output text. There is currently no way to get these
  //   mappings out anyway so passing in anything other than `false` makes no
  //   sense. If `html` is `true`, it will internally be enabled as alignments
  //   are necessary for the transfer of markup to the translated HTML.
  // `html`: is the input HTML? If so, the HTML will be parsed and the markup
  //   will be copied back into the translated output. Note: HTML has to be
  //   valid HTML5, with proper closing tags and everything since the HTML
  //   parser built into bergamot-translator does no error correction. Output
  //   of e.g. `Element.innerHTML` meets this criteria.
  const options = new Module.VectorResponseOptions();
  options.push_back({qualityScores: false, alignment: false, html: true});

  // Size of `input` and `options` has to match.
  assert(input.size() === options.size());

  // Translate our batch (of 1). Output will be another vector of type 
  // `std::vector<Response>`.
  const output = service.translate(model, input, options);

  // Number of outputs is number of inputs.
  assert(input.size() === output.size());

  // Get output from std::vector<Response>.
  const translation = output.get(0).getTranslatedText();

  // Print raw translation for inspection.
  console.log(translation)

  // Response has more methods, but you generally don't need them. So those are
  // explained in a separate function.
  printSentences(output.get(0));

  // Clean-up
  input.delete();
  options.delete();
  output.delete();
}

/**
 * Demonstration of the other methods exposed by `Response`.
 * @arg {Module.Response} a single response from the translated batch.
 */
function printSentences(response) {
  // `Response` exposes more methods, such as `size()` to get the number of
  // sentences in a response, and `getSourceSentence()` and
  // `getTargetSentence()`. These return a `ByteRange` object that has a
  // `begin` and `end` field giving you the byte offset in
  // the string of `getOriginalText()` and `getTranslatedText()`.

  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  // Since sentence ranges are in byte offset (not characters!) we need to
  // convert our text back into byte arrays. bergamot-translator itself is
  // encoding agnostic, but the translation models are trained on UTF-8.
  const originalAsBytes = encoder.encode(response.getOriginalText());
  const translatedAsBytes = encoder.encode(response.getTranslatedText());

  // Note that if HTML is used, these slices will never slice in the middle of
  // a tag, but they might have an open tag in one sentence, and the
  // corresponding closing tag in another.
  for (let i = 0; i < response.size(); ++i) {
    let range = response.getSourceSentence(i);
    let slice = originalAsBytes.subarray(range.begin, range.end);
    console.log('Original sentence', i, ':', decoder.decode(slice));

    range = response.getTranslatedSentence(i);
    slice = translatedAsBytes.subarray(range.begin, range.end);
    console.log('Translated sentence', i, ':', decoder.decode(slice));
  }
}
