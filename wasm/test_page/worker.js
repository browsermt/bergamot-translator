var translationService, responseOptions, input = undefined;
const BERGAMOT_TRANSLATOR_MODULE = "bergamot-translator-worker.js";

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
        log(`Wasm Runtime initialized (preRun -> onRuntimeInitialized) in ${(Date.now() - moduleLoadStart) / 1000} secs`);
    }
};

const log = (message) => {
  console.debug(message);
}

onmessage = async function(e) {
    let command = e.data[0];
    log(`Message '${command}' received from main script`);
    let result = "";
    if (command === 'load_module') {
        importScripts(BERGAMOT_TRANSLATOR_MODULE);
        result = `Translator wasm module successfully loaded`;
        log(result);
        log('Posting message back to main script');
        postMessage(['module_loaded', result]);
    }
    else if (command === 'load_model') {
        let start = Date.now();
        await constructTranslationService(e.data[1], e.data[2]);
        result = `translation model '${e.data[1]}${e.data[2]}' successfully loaded; took ${(Date.now() - start) / 1000} secs`;
        log(result);
        log('Posting message back to main script');
        postMessage(['model_loaded', result]);
    }
    else if (command === 'translate') {
        const inputParagraphs = e.data[1];
        let inputWordCount = 0;
        inputParagraphs.forEach(sentence => {
            inputWordCount += sentence.trim().split(" ").filter(word => word.trim() !== "").length;
        })

        let start = Date.now();
        const translatedParagraphs = translate(e.data[1]);
        const secs = (Date.now() - start) / 1000;
        result = `Translation of (${inputWordCount}) words took ${secs} secs (${Math.round(inputWordCount / secs)} words per second)`;
        log(result);
        log('Posting message back to main script');
        postMessage(['translated_result', translatedParagraphs, result]);
    }
}

// This function downloads file from a url and returns the array buffer
const downloadAsArrayBuffer = async(url) => {
    const response = await fetch(url);
    if (!response.ok) {
        throw Error(`Downloading ${url} failed: HTTP ${response.status} - ${response.statusText}`);
    }
    return response.arrayBuffer();
}

// This function constructs and initializes the AlignedMemory from the array buffer and alignment size
const prepareAlignedMemoryFromBuffer = async (buffer, alignmentSize) => {
    var byteArray = new Int8Array(buffer);
    log(`Constructing Aligned memory with size: ${byteArray.byteLength} bytes with alignment: ${alignmentSize}`);
    var alignedMemory = new Module.AlignedMemory(byteArray.byteLength, alignmentSize);
    log(`Aligned memory construction done`);
    const alignedByteArrayView = alignedMemory.getByteArrayView();
    alignedByteArrayView.set(byteArray);
    log(`Aligned memory initialized`);
    return alignedMemory;
}

const constructTranslationService = async (from, to) => {
    const languagePair = `${from}${to}`;

    // Vocab files are re-used in both translation directions
    const vocabLanguagePair = from === "en" ? `${to}${from}` : languagePair;

    // Set the Model Configuration as YAML formatted string.
    // For available configuration options, please check: https://marian-nmt.github.io/docs/cmd/marian-decoder/
    /*const modelConfig = `models:
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
gemm-precision: int8shift
`;

    const modelFile = `models/${languagePair}/model.${languagePair}.intgemm.alphas.bin`;
    const shortlistFile = `models/${languagePair}/lex.50.50.${languagePair}.s2t.bin`;
    const vocabFiles = [`models/${languagePair}/vocab.${vocabLanguagePair}.spm`,
                        `models/${languagePair}/vocab.${vocabLanguagePair}.spm`];

    const uniqueVocabFiles = new Set(vocabFiles);
    log(`modelFile: ${modelFile}\nshortlistFile: ${shortlistFile}\nNo. of unique vocabs: ${uniqueVocabFiles.size}`);
    uniqueVocabFiles.forEach(item => log(`unique vocabFile: ${item}`));

    try {
      // Download the files as buffers from the given urls
        let start = Date.now();
        const downloadedBuffers = await Promise.all([downloadAsArrayBuffer(modelFile), downloadAsArrayBuffer(shortlistFile)]);
        const modelBuffer = downloadedBuffers[0];
        const shortListBuffer = downloadedBuffers[1];

        const downloadedVocabBuffers = [];
        for (let item of uniqueVocabFiles.values()) {
            downloadedVocabBuffers.push(await downloadAsArrayBuffer(item));
        }
        log(`All files for ${languagePair} language pair took ${(Date.now() - start) / 1000} secs to download`);

        // Construct AlignedMemory objects with downloaded buffers
        let constructedAlignedMemories = await Promise.all([prepareAlignedMemoryFromBuffer(modelBuffer, 256),
                                                                prepareAlignedMemoryFromBuffer(shortListBuffer, 64)]);
        let alignedModelMemory = constructedAlignedMemories[0];
        let alignedShortlistMemory = constructedAlignedMemories[1];
        let alignedVocabsMemoryList = new Module.AlignedMemoryList;
        for(let item of downloadedVocabBuffers) {
            let alignedMemory = await prepareAlignedMemoryFromBuffer(item, 64);
            alignedVocabsMemoryList.push_back(alignedMemory);
        }
        log(`Aligned vocab memories: ${alignedVocabsMemoryList.get(0).size()}`);
        log(`Aligned model memory: ${alignedModelMemory.size()}`);
        log(`Aligned shortlist memory: ${alignedShortlistMemory.size()}`);

        // Instantiate the Translation Service
        if (translationService) {
            translationService.delete();
            translationService = undefined;
        }

        log(`Creating Translation Service with config: ${modelConfig}`);
        translationService = new Module.Service(modelConfig, alignedModelMemory, alignedShortlistMemory, alignedVocabsMemoryList);
        if (typeof translationService === 'undefined') {
            throw Error(`Translation Service construction failed`);
        }
    } catch (error) {
        log(error);
    }
  }

const translate = (paragraphs) => {
    // Instantiate the arguments of translate() API i.e. ResponseOptions and input (vector<string>)
    var responseOptions = new Module.ResponseOptions();
    let input = new Module.VectorString;

    // Initialize the input
    paragraphs.forEach(paragraph => {
      // prevent empty paragraph - it breaks the translation
        if (paragraph.trim() === "") {
           return;
        }
        input.push_back(paragraph.trim())
    })
    // Access input (just for debugging)
    log(`Input size: ${input.size()}`);

    // Translate the input, which is a vector<String>; the result is a vector<Response>
    let result = translationService.translate(input, responseOptions);

    const translatedParagraphs = [];
    const translatedSentencesOfParagraphs = [];
    const sourceSentencesOfParagraphs = [];
    for (let i = 0; i < result.size(); i++) {
        translatedParagraphs.push(result.get(i).getTranslatedText());
        translatedSentencesOfParagraphs.push(getAllTranslatedSentencesOfParagraph(result.get(i)));
        sourceSentencesOfParagraphs.push(getAllSourceSentencesOfParagraph(result.get(i)));
    }
    log({ translatedParagraphs });
    log({ translatedSentencesOfParagraphs });
    log({ sourceSentencesOfParagraphs });

    responseOptions.delete();
    input.delete();
    return translatedParagraphs;
}

// This function extracts all the translated sentences from the Response and returns them.
const getAllTranslatedSentencesOfParagraph = (response) => {
    const sentences = [];
    const text = response.getTranslatedText();
    for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
        const utf8SentenceByteRange = response.getTranslatedSentence(sentenceIndex);
        sentences.push(_getSentenceFromByteRange(text, utf8SentenceByteRange));
    }
    return sentences;
}

// This function extracts all the source sentences from the Response and returns them.
const getAllSourceSentencesOfParagraph = (response) => {
    const sentences = [];
    const text = response.getOriginalText();
    for (let sentenceIndex = 0; sentenceIndex < response.size(); sentenceIndex++) {
        const utf8SentenceByteRange = response.getSourceSentence(sentenceIndex);
        sentences.push(_getSentenceFromByteRange(text, utf8SentenceByteRange));
    }
    return sentences;
}

// This function returns a substring of text (a string). The substring is represented by
// byteRange (begin and end endices) within the utf-8 encoded version of the text.
const _getSentenceFromByteRange = (text, byteRange) => {
    const utf8BytesView = encoder.encode(text);
    const utf8SentenceBytes = utf8BytesView.subarray(byteRange.begin, byteRange.end);
    return decoder.decode(utf8SentenceBytes);
}
