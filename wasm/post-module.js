/* global addOnPreMain, Module */

addOnPreMain(function() {
  let model;

  const loadModel = () => {
    // Delete previous instance if another model was loaded
    if (model) {
      model.delete();
    }

    // Instantiate the TranslationModelConfiguration
    const modelConfig = new Module.TranslationModelConfiguration(
      "dummy_modelFilePath",
      "dummy_sourceVocabPath",
      "dummy_targetVocabPath",
    );
    console.debug("Model Path:", modelConfig.getModelFilePath());

    // Instantiate the TranslationModel
    model = new Module.TranslationModel(modelConfig);

    const alignmentIsSupported = model.isAlignmentSupported();
    console.debug("Alignment:", alignmentIsSupported);

    // Clean up the instances
    modelConfig.delete();

    return { alignmentIsSupported };
  };

  /**
   * @param texts string[]
   */
  const translate = texts => {
    if (!model) {
      throw new Error("Translate attempted before model was loaded");
    }

    // TODO: Check that the loaded model supports the translation direction

    // Instantiate the arguments of translate() API i.e. TranslationRequest and input (vector<string>)
    const request = new Module.TranslationRequest();
    const input = new Module.VectorString();

    // Initialize the input
    texts.forEach(text => input.push_back(text));

    // Access input (just for debugging)
    console.debug("Input size=", input.size());
    for (let i = 0; i < input.size(); i++) {
      console.debug(" val:" + input.get(i));
    }

    // translate the input; the result is a vector<TranslationResult>
    const result = model.translate(input, request);

    // Access input after translation (just for debugging)
    console.debug("Input size after translate API call =", input.size());
    for (let i = 0; i < input.size(); i++) {
      console.debug(" val:" + input.get(i));
    }

    // Access original and translated text from each entry of vector<TranslationResult>
    console.debug("Result size=", result.size());
    const translationResults = {
      originalTexts: [],
      translatedTexts: [],
    };
    for (let i = 0; i < result.size(); i++) {
      const originalText = result.get(i).getOriginalText();
      const translatedText = result.get(i).getTranslatedText();
      console.debug(
        " original={" +
        originalText +
        "}, translation={" +
        translatedText +
        "}",
      );
      translationResults.originalTexts.push(originalText);
      translationResults.translatedTexts.push(translatedText);
    }

    // Clean up the instances
    request.delete();
    input.delete();

    return translationResults;
  };

  onmessage = function(msg) {
    const { data } = msg;
    if (!data.type || !data.requestId) {
      return;
    }
    const requestId = data.requestId;
    if (data.type === "loadModel") {
      const loadModelResults = loadModel();
      postMessage({
        type: "loadModelResults",
        requestId,
        loadModelResults,
      });
    } else if (data.type === "translate") {
      const translationResults = translate(data.translateParams.texts);
      postMessage({
        type: "translationResults",
        requestId,
        translationResults,
      });
    } else {
      throw new Error(
        `Unexpected message type: "${data.type}. Request id: ${data.requestId}"`,
      );
    }
  };

  // Send a message indicating that the worker is ready to receive WASM-related messages
  postMessage("ready");
});
