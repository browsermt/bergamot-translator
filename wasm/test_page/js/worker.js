// All variables specific to translation service
var translationService = undefined;
// A map of language-pair to TranslationModel object
var languagePairToTranslationModels = new Map();

const BERGAMOT_TRANSLATOR_MODULE = "bergamot-translator-worker.js";
const MODEL_REGISTRY = "modelRegistry.js";

const encoder = new TextEncoder(); // string to utf-8 converter
const decoder = new TextDecoder(); // utf-8 to string converter

const start = Date.now();
let moduleLoadStart;
var Module = {
  preRun: [function() {
    log(`Time until Module.preRun: ${(Date.now() - start) / 1000} secs`);
    moduleLoadStart = Date.now();
  }],
  onRuntimeInitialized: function() {
    log(`Wasm Runtime initialized Successfully (preRun -> onRuntimeInitialized) in ${(Date.now() - moduleLoadStart) / 1000} secs`);
    importScripts(MODEL_REGISTRY);
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
      let inputWordCount = 0;
      input.forEach(sentence => {
        inputWordCount += sentence.trim().split(" ").filter(word => word.trim() !== "").length;
      })
      let start = Date.now();
      try {
        result = translate(from, to, input);
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
    var translationServiceConfig = {};
    log(`Creating Translation Service with config: ${translationServiceConfig}`);
    translationService = new Module.BlockingService(translationServiceConfig);
    log(`Translation Service created successfully`);
  }
}

// Constructs a translation model object for the source and target language pair
const constructTranslationModel = async (from, to) => {
  // Delete all previously constructed translation models and clear the map
  languagePairToTranslationModels.forEach((value, key) => {
    log(`Destructing model '${key}'`);
    value.delete();
  });
  languagePairToTranslationModels.clear();

  // If none of the languages is English then construct multiple models with
  // English as a pivot language.
  if (from !== 'en' && to !== 'en') {
    log(`Constructing model '${from}${to}' via pivoting: '${from}en' and 'en${to}'`);
    await Promise.all([_constructTranslationModelInvolvingEnglish(from, 'en'),
                        _constructTranslationModelInvolvingEnglish('en', to)]);
  }
  else {
    log(`Constructing model '${from}${to}'`);
    await _constructTranslationModelInvolvingEnglish(from, to);
  }
}

// Translates text from source language to target language.
const translate = (from, to, input) => {
  // If none of the languages is English then perform translation with
  // English as a pivot language.
  if (from !== 'en' && to !== 'en') {
    log(`Translating '${from}${to}' via pivoting: '${from}en' -> 'en${to}'`);
    let translatedTextInEnglish = _translateInvolvingEnglish(from, 'en', input);
    return _translateInvolvingEnglish('en', to, translatedTextInEnglish);
  }
  else {
    log(`Translating '${from}${to}'`);
    return _translateInvolvingEnglish(from, to, input);
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
  log(`Constructing Aligned memory. Size: ${byteArray.byteLength} bytes, Alignment: ${alignmentSize}`);
  var alignedMemory = new Module.AlignedMemory(byteArray.byteLength, alignmentSize);
  log(`Aligned memory construction done`);
  const alignedByteArrayView = alignedMemory.getByteArrayView();
  alignedByteArrayView.set(byteArray);
  log(`Aligned memory initialized`);
  return alignedMemory;
}

const _constructTranslationModelInvolvingEnglish = async (from, to) => {
  const languagePair = `${from}${to}`;

  /*Set the Model Configuration as YAML formatted string.
    For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
    Vocab files are re-used in both translation directions
    const vocabLanguagePair = from === "en" ? `${to}${from}` : languagePair;
    const modelConfig = `models:
      - /${languagePair}/model.${languagePair}.intgemm.alphas.bin
      vocabs:
      - /${languagePair}/vocab.${vocabLanguagePair}.spm
      - /${languagePair}/vocab.${vocabLanguagePair}.spm
      beam-size: 1
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
      shortlist:
          - /${languagePair}/lex.${languagePair}.s2t
          - 50
          - 50
      `;
      */

  // TODO: gemm-precision: int8shiftAlphaAll (for the models that support this)
  // DONOT CHANGE THE SPACES BETWEEN EACH ENTRY OF CONFIG
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
gemm-precision: int8shiftAlphaAll
`;

  const modelFile = `${rootURL}/${languagePair}/${modelRegistry[languagePair]["model"].name}`;
  const shortlistFile = `${rootURL}/${languagePair}/${modelRegistry[languagePair]["lex"].name}`;
  const vocabFiles = [`${rootURL}/${languagePair}/${modelRegistry[languagePair]["vocab"].name}`,
                      `${rootURL}/${languagePair}/${modelRegistry[languagePair]["vocab"].name}`];

  const uniqueVocabFiles = new Set(vocabFiles);
  log(`modelFile: ${modelFile}\nshortlistFile: ${shortlistFile}\nNo. of unique vocabs: ${uniqueVocabFiles.size}`);
  uniqueVocabFiles.forEach(item => log(`unique vocabFile: ${item}`));

  // Download the files as buffers from the given urls
  let start = Date.now();
  const downloadedBuffers = await Promise.all([_downloadAsArrayBuffer(modelFile), _downloadAsArrayBuffer(shortlistFile)]);
  const modelBuffer = downloadedBuffers[0];
  const shortListBuffer = downloadedBuffers[1];

  const downloadedVocabBuffers = [];
  for (let item of uniqueVocabFiles.values()) {
    downloadedVocabBuffers.push(await _downloadAsArrayBuffer(item));
  }
  log(`Total Download time for all files of '${languagePair}': ${(Date.now() - start) / 1000} secs`);

  // Construct AlignedMemory objects with downloaded buffers
  let constructedAlignedMemories = await Promise.all([_prepareAlignedMemoryFromBuffer(modelBuffer, 256),
                                                      _prepareAlignedMemoryFromBuffer(shortListBuffer, 64)]);
  let alignedModelMemory = constructedAlignedMemories[0];
  let alignedShortlistMemory = constructedAlignedMemories[1];
  let alignedVocabsMemoryList = new Module.AlignedMemoryList;
  for(let item of downloadedVocabBuffers) {
    let alignedMemory = await _prepareAlignedMemoryFromBuffer(item, 64);
    alignedVocabsMemoryList.push_back(alignedMemory);
  }
  for (let vocabs=0; vocabs < alignedVocabsMemoryList.size(); vocabs++) {
    log(`Aligned vocab memory${vocabs+1} size: ${alignedVocabsMemoryList.get(vocabs).size()}`);
  }
  log(`Aligned model memory size: ${alignedModelMemory.size()}`);
  log(`Aligned shortlist memory size: ${alignedShortlistMemory.size()}`);

  log(`Translation Model config: ${modelConfig}`);
  var translationModel = new Module.TranslationModel(modelConfig, alignedModelMemory, alignedShortlistMemory, alignedVocabsMemoryList);
  languagePairToTranslationModels.set(languagePair, translationModel);
}

const _translateInvolvingEnglish = (from, to, input) => {
  const languagePair = `${from}${to}`;
  if (!languagePairToTranslationModels.has(languagePair)) {
    throw Error(`Please load translation model '${languagePair}' before translating`);
  }
  translationModel = languagePairToTranslationModels.get(languagePair);

  // Prepare the arguments of translate() API i.e. ResponseOptions and listSourceText (vector<string>)
  var responseOptions = {qualityScores: true, alignment: false, alignmentThreshold: 0.2};
  let listSourceText = new Module.VectorString;

  // Initialize the listSourceText
  input.forEach(paragraph => {
    // prevent empty paragraph - it breaks the translation
    if (paragraph.trim() === "") {
      return;
    }
    listSourceText.push_back(paragraph.trim())
  })

  // Access listSourceText (just for debugging)
  log(`Size of source text list: ${listSourceText.size()}`);

  // Translate the listSourceText, which is a vector<String>; the result is a vector<Response>
  let result = translationService.translate(translationModel, listSourceText, responseOptions);

  const listTranslatedText = [];
  const listTranslatedTextSentences = [];
  const listSourceTextSentences = [];
  const listTranslatedTextSentenceQualityScores = [];
  for (let i = 0; i < result.size(); i++) {
    let response = result.get(i);
    listTranslatedText.push(response.getTranslatedText());
    listTranslatedTextSentences.push(_getAllTranslatedSentencesOfParagraph(response));
    listSourceTextSentences.push(_getAllSourceSentencesOfParagraph(response));
    let sentenceQualityScores = response.getQualityScores();
    log(`No. of sentences: "${sentenceQualityScores.size()}"`);
    const stringifiedSentenceQualityScores = [];
    for (let sentenceIndex=0; sentenceIndex < sentenceQualityScores.size(); sentenceIndex++) {
      let sentenceQualityScore = sentenceQualityScores.get(sentenceIndex);
      stringifiedSentenceQualityScores.push(sentenceQualityScore);
    }
    listTranslatedTextSentenceQualityScores.push(stringifiedSentenceQualityScores);
  }

  log(`Translated sentences: ${JSON.stringify(listTranslatedTextSentences)}`);
  log(`Source sentences: ${JSON.stringify(listSourceTextSentences)}`);
  log(`Translated sentence quality scores: ${JSON.stringify(listTranslatedTextSentenceQualityScores)}`);
  listSourceText.delete();
  return listTranslatedText;
}

// Extracts all the translated sentences from the Response and returns them.
const _getAllTranslatedSentencesOfParagraph = (response) => {
  const sentences = [];
  const text = response.getTranslatedText();
  for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
    const utf8SentenceByteRange = response.getTranslatedSentence(sentenceIndex);
    sentences.push(_getSentenceFromByteRange(text, utf8SentenceByteRange));
  }
  return sentences;
}

// Extracts all the source sentences from the Response and returns them.
const _getAllSourceSentencesOfParagraph = (response) => {
  const sentences = [];
  const text = response.getOriginalText();
  for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
    const utf8SentenceByteRange = response.getSourceSentence(sentenceIndex);
    sentences.push(_getSentenceFromByteRange(text, utf8SentenceByteRange));
  }
  return sentences;
}

/*
 * Returns a substring of text (a string). The substring is represented by
 * byteRange (begin and end endices) within the utf-8 encoded version of the text.
 */
const _getSentenceFromByteRange = (text, byteRange) => {
  const utf8BytesView = encoder.encode(text);
  const utf8SentenceBytes = utf8BytesView.subarray(byteRange.begin, byteRange.end);
  return decoder.decode(utf8SentenceBytes);
}
