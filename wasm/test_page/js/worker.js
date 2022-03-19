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

// Information corresponding to each file type
const fileInfo = [
  {"type": "model", "alignment": 256},
  {"type": "lex", "alignment": 64},
  {"type": "vocab", "alignment": 64},
  {"type": "qualityModel", "alignment": 64}
];

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
    postMessage([null, `import_reply`, modelRegistry, BERGAMOT_VERSION_FULL]);
  }
};

const log = (message) => {
  console.debug(message);
}

onmessage = async function(e) {
  const [id, command] = e.data.slice(0, 2);
  log(`Message '${command}' received from main script`);
  let result = "";
  if (command === 'import') {
      importScripts(BERGAMOT_TRANSLATOR_MODULE);
  } else if (command === 'load_model') {
      let start = Date.now();
      let [from, to] = e.data.slice(2, 2+2);
      try {
        await constructTranslationService();
        await constructTranslationModel(from, to, e.data[4]);
        log(`Model '${from}${to}' successfully constructed. Time taken: ${(Date.now() - start) / 1000} secs`);
        result = "Model successfully loaded";
      } catch (error) {
        log(`Model '${from}${to}' construction failed: '${error.message}'`);
        result = "Model loading failed";
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([id, `${command}_reply`, result]);
  } else if (command === 'translate') {
      const [from, to, input, options] = e.data.slice(2, 2+4);
      let inputWordCount = input.reduce((acc, text) => acc + text.trim().replace(/\<\/?.+?>/g, '').split(" ").filter(word => word.trim() !== "").length, 0);
      let start = performance.now();
      try {
        result = translate(from, to, input, options);
        const secs = (performance.now() - start) / 1000;
        log(`Translation '${from}${to}' Successful. Speed: ${Math.round(inputWordCount / secs)} WPS (${inputWordCount} words in ${secs} secs)`);
      } catch (error) {
        log(`Error: ${error.message}`);
        postMessage([id, 'error', error.message]);
      }
      log(`'${command}' command done, Posting message back to main script`);
      postMessage([id, `${command}_reply`, result]);
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
const constructTranslationModel = async (from, to, files) => {
  // Delete all previously constructed translation models and clear the map
  languagePairToTranslationModels.forEach((value, key) => {
    log(`Destructing model '${key}'`);
    value.delete();
  });
  languagePairToTranslationModels.clear();

  if (_isPivotingRequired(from, to) & files === undefined) {
    // Pivoting requires 2 translation models
    const languagePairSrcToPivot = _getLanguagePair(from, PIVOT_LANGUAGE);
    const languagePairPivotToTarget = _getLanguagePair(PIVOT_LANGUAGE, to);
    await Promise.all([_constructTranslationModelHelper(languagePairSrcToPivot),
                      _constructTranslationModelHelper(languagePairPivotToTarget)]);
  }
  else {
    // Non-pivoting case requires only 1 translation model
    await _constructTranslationModelHelper(_getLanguagePair(from, to), files);
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
    const listAlignments = _parseAlignments(vectorResponse);

    log(`Source text: ${listSourceText}`);
    log(`Translated text: ${listTranslatedText}`);
    log(`Translated sentences: ${JSON.stringify(listTranslatedTextSentences)}`);
    log(`Source sentences: ${JSON.stringify(listSourceTextSentences)}`);

    return {
      source: listSourceText[0],
      translated: listTranslatedText[0],
      alignments: listAlignments[0],
    };
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

const _readAsArrayBuffer = async (file) => {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = (e) => resolve(e.target.result);
    reader.onerror = (e) => reject(e.target.error);
    reader.readAsArrayBuffer(file);
  });
}

// Constructs and initializes the AlignedMemory from the array buffer and alignment size
const _prepareAlignedMemoryFromBuffer = async (buffer, alignmentSize) => {
  var byteArray = new Int8Array(buffer);
  var alignedMemory = new Module.AlignedMemory(byteArray.byteLength, alignmentSize);
  const alignedByteArrayView = alignedMemory.getByteArrayView();
  alignedByteArrayView.set(byteArray);
  return alignedMemory;
}

function unique(items) {
  return new Set(items).values();
}

async function prepareAlignedMemory(file, languagePair) {
  const fileName = `${MODEL_ROOT_URL}/${languagePair}/${modelRegistry[languagePair][file.type].name}`;
  const buffer = await _downloadAsArrayBuffer(fileName);
  const alignedMemory = await _prepareAlignedMemoryFromBuffer(buffer, file.alignment);
  log(`"${file.type}" aligned memory prepared. Size:${alignedMemory.size()} bytes, alignment:${file.alignment}`);
  return alignedMemory;
}

const _constructTranslationModelHelper = async (languagePair, files) => {
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

  let promises = [];

  if (files === undefined) {
    promises = fileInfo.filter(file => modelRegistry[languagePair].hasOwnProperty(file.type))
    .map((file) => prepareAlignedMemory(file, languagePair));
  } else {
    promises = [files.modelFile, files.shortlistFile, ...unique(files.vocabFiles)]
    .map(async (file, i) => _prepareAlignedMemoryFromBuffer(await _readAsArrayBuffer(file), fileInfo[i].alignment));
  }

  const alignedMemories = await Promise.all(promises);

  log(`Translation Model config: ${modelConfig}`);
  log(`Aligned memory sizes: Model:${alignedMemories[0].size()} Shortlist:${alignedMemories[1].size()} Vocab:${alignedMemories[2].size()}`);
  const alignedVocabMemoryList = new Module.AlignedMemoryList();
  alignedVocabMemoryList.push_back(alignedMemories[2]);
  let translationModel;
  if (alignedMemories.length === fileInfo.length) {
    log(`QE:${alignedMemories[3].size()}`);
    translationModel = new Module.TranslationModel(modelConfig, alignedMemories[0], alignedMemories[1], alignedVocabMemoryList, alignedMemories[3]);
  }
  else {
    translationModel = new Module.TranslationModel(modelConfig, alignedMemories[0], alignedMemories[1], alignedVocabMemoryList, null);
  }
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
    const htmlOptions = new Module.HTMLOptions();

    if ("htmlOptions" in translateOption) {
      Object.entries(translateOption["htmlOptions"]).forEach(([option, value]) => {
        htmlOptions[`set${option.substr(0, 1).toUpperCase()}${option.substr(1)}`](value);
      });
    }

    vectorResponseOptions.push_back({
      qualityScores: translateOption["isQualityScores"],
      alignment: true,
      html: translateOption["isHtml"],
      htmlOptions
    });
  });
  if (vectorResponseOptions.size() == 0) {
    vectorResponseOptions.delete();
    throw Error(`No Translation Options provided`);
  }

  return vectorResponseOptions;
}

const _parseAlignments = (vectorResponse) => {
  const result = [];

  for (let i = 0; i < vectorResponse.size(); i++) {
    const response = vectorResponse.get(i);

    const original = response.getOriginalText();
    const translated = response.getTranslatedText();

    const sentences = [];

    for (let sentenceIndex = 0; sentenceIndex < response.size(); ++sentenceIndex) {
      const originalTokens = [];
      const translatedTokens = [];
      const scores = []; // scores[translated:int][original:int] = score:float

      for (let wordIndex = 0; wordIndex < response.getSourceSentenceSize(sentenceIndex); ++wordIndex) {
        originalTokens.push(_getSubString(original, response.getSourceWord(sentenceIndex, wordIndex)));
      }

      for (let wordIndex = 0; wordIndex < response.getTranslatedSentenceSize(sentenceIndex); ++wordIndex) {
        translatedTokens.push(_getSubString(translated, response.getTranslatedWord(sentenceIndex, wordIndex)));
      }

      for (let t = 0; t < response.getTranslatedSentenceSize(sentenceIndex); ++t) {
        scores.push([]);
        for (let s = 0; s < response.getSourceSentenceSize(sentenceIndex); ++s) {
          scores[t].push(response.getAlignmentScore(sentenceIndex, t, s));
        }
      }

      sentences.push({originalTokens, translatedTokens, scores})
    }
    result.push(sentences);
  }

  return result;
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
