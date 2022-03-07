// All variables specific to translation service
var translationService = undefined;

// Model registry
let modelRegistry = undefined;

// A map of language-pair to TranslationModel object
var languagePairToTranslationModels = new Map();

const BERGAMOT_TRANSLATOR_MODULE = "bergamot-translator-worker.js";
const MODEL_REGISTRY = "../models/registry.json";
const MODEL_ROOT_URL = "../models/";
const PIVOT_LANGUAGE = 'en';

const encoder = new TextEncoder(); // string to utf-8 converter
const decoder = new TextDecoder(); // utf-8 to string converter

const start = Date.now();
let moduleLoadStart;
var Module = {
  preRun: [function() {
    log(`Time until Module.preRun: ${(Date.now() - start) / 1000} secs`);
    moduleLoadStart = Date.now();
  }],
  onRuntimeInitialized: async function() {
    log(`Wasm Runtime initialized Successfully (preRun -> onRuntimeInitialized) in ${(Date.now() - moduleLoadStart) / 1000} secs`);
    const response = await fetch(MODEL_REGISTRY);
    modelRegistry = await response.json();
    postMessage([`import_reply`, modelRegistry]);
  }
};

const log = (message) => {
  console.debug(message);
}

onmessage = async function(e) {
  const command = e.data[0];
  log(`Message '${command}' received from main script`);
  let result = "";
  if (command === 'import') {
      importScripts(BERGAMOT_TRANSLATOR_MODULE);
  } else if (command === 'load_model') {
      let start = Date.now();
      let from = e.data[1];
      let to = e.data[2];
      try {
        await constructTranslationService();
        await constructTranslationModel(from, to);
        log(`Model '${from}${to}' successfully constructed. Time taken: ${(Date.now() - start) / 1000} secs`);
        result = "Model successfully loaded";
      } catch (error) {
        log(`Model '${from}${to}' construction failed: '${error.message}'`);
        result = "Model loading failed";
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([`${command}_reply`, result]);
  } else if (command === 'translate') {
      const from = e.data[1];
      const to = e.data[2];
      const input = e.data[3];
      const translateOptions = e.data[4];
      let inputWordCount = 0;
      let inputBlockElements = 0;
      input.forEach(sentence => {
        inputWordCount += sentence.trim().split(" ").filter(word => word.trim() !== "").length;
        inputBlockElements++;
      })
      let start = Date.now();
      try {
        log(`Blocks to translate: ${inputBlockElements}`);
        result = translate(from, to, input, translateOptions);
        const secs = (Date.now() - start) / 1000;
        log(`Translation '${from}${to}' Successful. Speed: ${Math.round(inputWordCount / secs)} WPS (${inputWordCount} words in ${secs} secs)`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([`${command}_reply`, result]);
  }
}

// Instantiates the Translation Service
const constructTranslationService = async () => {
  if (!translationService) {
    var translationServiceConfig = {cacheSize: 20000};
    log(`Creating Translation Service with config: ${translationServiceConfig}`);
    translationService = new Module.BlockingService(translationServiceConfig);
    log(`Translation Service created successfully`);
  }
}

// Constructs translation model(s) for the source and target language pair (using
// pivoting if required).
const constructTranslationModel = async (from, to) => {
  // Delete all previously constructed translation models and clear the map
  languagePairToTranslationModels.forEach((value, key) => {
    log(`Destructing model '${key}'`);
    value.delete();
  });
  languagePairToTranslationModels.clear();

  if (_isPivotingRequired(from, to)) {
    // Pivoting requires 2 translation models
    const languagePairSrcToPivot = _getLanguagePair(from, PIVOT_LANGUAGE);
    const languagePairPivotToTarget = _getLanguagePair(PIVOT_LANGUAGE, to);
    await Promise.all([_constructTranslationModelHelper(languagePairSrcToPivot),
                      _constructTranslationModelHelper(languagePairPivotToTarget)]);
  }
  else {
    // Non-pivoting case requires only 1 translation model
    await _constructTranslationModelHelper(_getLanguagePair(from, to));
  }
}

// Translates text from source language to target language (via pivoting if necessary).
const translate = (from, to, input, translateOptions) => {
  let vectorResponseOptions, vectorSourceText, vectorResponse;
  try {
    // Prepare the arguments (vectorResponseOptions and vectorSourceText (vector<string>)) of Translation API and call it.
    // Result is a vector<Response> where each of its item corresponds to one item of vectorSourceText in the same order.
    vectorResponseOptions = _prepareResponseOptions(translateOptions);
    vectorSourceText = _prepareSourceText(input);

    if (_isPivotingRequired(from, to)) {
      // Translate via pivoting
      const translationModelSrcToPivot = _getLoadedTranslationModel(from, PIVOT_LANGUAGE);
      const translationModelPivotToTarget = _getLoadedTranslationModel(PIVOT_LANGUAGE, to);
      vectorResponse = translationService.translateViaPivoting(translationModelSrcToPivot,
                                                              translationModelPivotToTarget,
                                                              vectorSourceText,
                                                              vectorResponseOptions);
    }
    else {
      // Translate without pivoting
      const translationModel = _getLoadedTranslationModel(from, to);
      vectorResponse = translationService.translate(translationModel, vectorSourceText, vectorResponseOptions);
    }

    // Parse all relevant information from vectorResponse
    const listTranslatedText = _parseTranslatedText(vectorResponse);
    const listSourceText = _parseSourceText(vectorResponse);
    const listTranslatedTextSentences = _parseTranslatedTextSentences(vectorResponse);
    const listSourceTextSentences = _parseSourceTextSentences(vectorResponse);

    log(`Source text: ${listSourceText}`);
    log(`Translated text: ${listTranslatedText}`);
    log(`Translated sentences: ${JSON.stringify(listTranslatedTextSentences)}`);
    log(`Source sentences: ${JSON.stringify(listSourceTextSentences)}`);

    return listTranslatedText;
  } finally {
    // Necessary clean up
    if (vectorSourceText != null) vectorSourceText.delete();
    if (vectorResponseOptions != null) vectorResponseOptions.delete();
    if (vectorResponse != null) vectorResponse.delete();
  }
}

// Downloads file from a url and returns the array buffer
const _downloadAsArrayBuffer = async(url) => {
  const response = await fetch(url);
  if (!response.ok) {
    throw Error(`Downloading ${url} failed: HTTP ${response.status} - ${response.statusText}`);
  }
  return response.arrayBuffer();
}

// Constructs and initializes the AlignedMemory from the array buffer and alignment size
const _prepareAlignedMemoryFromBuffer = async (buffer, alignmentSize) => {
  var byteArray = new Int8Array(buffer);
  var alignedMemory = new Module.AlignedMemory(byteArray.byteLength, alignmentSize);
  const alignedByteArrayView = alignedMemory.getByteArrayView();
  alignedByteArrayView.set(byteArray);
  log(`Aligned memory construction done. Size: ${byteArray.byteLength} bytes, Alignment: ${alignmentSize}`);
  return alignedMemory;
}

async function prepareAlignedMemory(file, languagePair) {
  const fileName = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair][file.type].name}`;
  const buffer = await _downloadAsArrayBuffer(fileName);
  const alignedMemory = await _prepareAlignedMemoryFromBuffer(buffer, file.alignment);
  log(`"${file.type}" aligned memory prepared. Size:${alignedMemory.size()} bytes, alignment:${file.alignment}`);
  return alignedMemory;
  //return {${file.type}: alignedMemory;
}

const _constructTranslationModelHelper = async (languagePair) => {
  log(`Constructing translation model ${languagePair}`);

  /*Set the Model Configuration as YAML formatted string.
    For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
    Vocab files are re-used in both translation directions.
    DO NOT CHANGE THE SPACES BETWEEN EACH ENTRY OF CONFIG
  */
  const modelConfig = `beam-size: 1
normalize: 1.0
word-penalty: 0
max-length-break: 128
mini-batch-words: 1024
workspace: 128
max-length-factor: 2.0
skip-cost: false
cpu-threads: 0
quiet: true
quiet-translation: true
gemm-precision: int8shiftAlphaAll
alignment: soft
`;

  const files = [
    {"type": "model", "alignment": 256},
    {"type": "lex", "alignment": 64},
    {"type": "vocab", "alignment": 64},
    {"type": "qe", "alignment": 64}
  ];
  const promises = [];
  files.filter(file => modelRegistry[languagePair].hasOwnProperty(file.type))
  .map((file) => {
      promises.push(prepareAlignedMemory(file, languagePair));
  });

  const alignedMemories = await Promise.all(promises);
  /*
  const modelFile = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair]["model"].name}`;
  const shortlistFile = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair]["lex"].name}`;
  const vocabFile = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair]["vocab"].name}`;
  const qeFile = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair]["qe"].name}`;
  log(`modelFile: ${modelFile}\nshortlistFile: ${shortlistFile}\nvocabFile: ${vocabFile}`);

  // Download the files as buffers from the given urls
  let start = Date.now();
  const downloadedBuffers = await Promise.all([
    _downloadAsArrayBuffer(modelFile),
    _downloadAsArrayBuffer(shortlistFile),
    _downloadAsArrayBuffer(vocabFile)
  ]);
  log(`Total Download time for all files of '${languagePair}': ${(Date.now() - start) / 1000} secs`);

  // Construct AlignedMemory objects with downloaded buffers
  let alignedMemories = await Promise.all([
    _prepareAlignedMemoryFromBuffer(downloadedBuffers[0], 256),
    _prepareAlignedMemoryFromBuffer(downloadedBuffers[1], 64),
    _prepareAlignedMemoryFromBuffer(downloadedBuffers[2], 64)
  ]);*/
  log(`Aligned memory sizes: Model:${alignedMemories[0].size()} Shortlist:${alignedMemories[1].size()} Vocab:${alignedMemories[2].size()}`);

  log(`Translation Model config: ${modelConfig}`);
  const alignedVocabMemoryList = new Module.AlignedMemoryList();
  alignedVocabMemoryList.push_back(alignedMemories[2]);
  var translationModel = new Module.TranslationModel(modelConfig, alignedMemories[0], alignedMemories[1], alignedVocabMemoryList);
  languagePairToTranslationModels.set(languagePair, translationModel);
}

const _isPivotingRequired = (from, to) => {
  return (from !== PIVOT_LANGUAGE) && (to !== PIVOT_LANGUAGE);
}

const _getLanguagePair = (srcLang, tgtLang) => {
  return `${srcLang}${tgtLang}`;
}

const _getLoadedTranslationModel = (srcLang, tgtLang) => {
  const languagePair = _getLanguagePair(srcLang, tgtLang);
  if (!languagePairToTranslationModels.has(languagePair)) {
    throw Error(`Translation model '${languagePair}' not loaded`);
  }
  return languagePairToTranslationModels.get(languagePair);
}

const _parseTranslatedText = (vectorResponse) => {
  const result = [];
  for (let i = 0; i < vectorResponse.size(); i++) {
    const response = vectorResponse.get(i);
    result.push(response.getTranslatedText());
  }
  return result;
}

const _parseTranslatedTextSentences = (vectorResponse) => {
  const result = [];
  for (let i = 0; i < vectorResponse.size(); i++) {
    const response = vectorResponse.get(i);
    result.push(_getTranslatedSentences(response));
  }
  return result;
}

const _parseSourceText = (vectorResponse) => {
  const result = [];
  for (let i = 0; i < vectorResponse.size(); i++) {
    const response = vectorResponse.get(i);
    result.push(response.getOriginalText());
  }
  return result;
}

const _parseSourceTextSentences = (vectorResponse) => {
  const result = [];
  for (let i = 0; i < vectorResponse.size(); i++) {
    const response = vectorResponse.get(i);
    result.push(_getSourceSentences(response));
  }
  return result;
}

const _prepareResponseOptions = (translateOptions) => {
  let vectorResponseOptions = new Module.VectorResponseOptions;
  translateOptions.forEach(translateOption => {
    vectorResponseOptions.push_back({
      qualityScores: translateOption["isQualityScores"],
      alignment: true,
      html: translateOption["isHtml"]
    });
  });
  if (vectorResponseOptions.size() == 0) {
    vectorResponseOptions.delete();
    throw Error(`No Translation Options provided`);
  }
  return vectorResponseOptions;
}

const _prepareSourceText = (input) => {
  let vectorSourceText = new Module.VectorString;
  input.forEach(paragraph => {
    // prevent empty paragraph - it breaks the translation
    if (paragraph.trim() === "") {
      return;
    }
    vectorSourceText.push_back(paragraph.trim())
  })
  if (vectorSourceText.size() == 0) {
    vectorSourceText.delete();
    throw Error(`No text provided to translate`);
  }
  return vectorSourceText;
}

const _getTranslatedSentences = (response) => {
  const sentences = [];
  const text = response.getTranslatedText();
  for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
    const utf8SentenceByteRange = response.getTranslatedSentence(sentenceIndex);
    sentences.push(_getSubString(text, utf8SentenceByteRange));
  }
  return sentences;
}

const _getSourceSentences = (response) => {
  const sentences = [];
  const text = response.getOriginalText();
  for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
    const utf8SentenceByteRange = response.getSourceSentence(sentenceIndex);
    sentences.push(_getSubString(text, utf8SentenceByteRange));
  }
  return sentences;
}

/*
 * Returns a substring of text (a string). The substring is represented by
 * byteRange (begin and end endices) within the utf-8 encoded version of the text.
 */
const _getSubString = (text, utf8ByteRange) => {
  const textUtf8ByteView = encoder.encode(text);
  const substringUtf8ByteView = textUtf8ByteView.subarray(utf8ByteRange.begin, utf8ByteRange.end);
  return decoder.decode(substringUtf8ByteView);
}
