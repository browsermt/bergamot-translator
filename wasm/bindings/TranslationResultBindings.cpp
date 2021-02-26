/*
 * Bindings for TranslationResult class
 *
 */

#include <emscripten/bind.h>
#include <vector>

#include "TranslationResult.h"

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_result) {
  class_<TranslationResult>("TranslationResult")
    .constructor<std::string, std::string, TranslationResult::SentenceMappings>()
	  .function("getOriginalText", &TranslationResult::getOriginalText)
	  .function("getTranslatedText", &TranslationResult::getTranslatedText)
    ;
}
