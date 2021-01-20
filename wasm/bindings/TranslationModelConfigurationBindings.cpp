/*
 * Bindings for TranslationModelConfiguration class
 *
 */

#include <emscripten/bind.h>

#include "TranslationModelConfiguration.h"

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_model_configuration) {
  class_<TranslationModelConfiguration>("TranslationModelConfiguration")
    .constructor<std::string, std::string, std::string>()
    .function("getModelFilePath", &TranslationModelConfiguration::getModelFilePath)
	  .function("getSourceVocabularyPath", &TranslationModelConfiguration::getSourceVocabularyPath)
    ;
}
