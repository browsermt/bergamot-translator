/*
 * Bindings for TranslationRequest class
 *
 */

#include <emscripten/bind.h>

#include "response_options.h"

typedef marian::bergamot::ResponseOptions TranslationRequest;

using namespace emscripten;

TranslationRequest *TranslationRequestFactory(std::string configYAML) {
  TranslationRequest *translationRequest = new TranslationRequest();
  *translationRequest = TranslationRequest::fromYAMLString(configYAML);
  return translationRequest;
}

// Binding code
EMSCRIPTEN_BINDINGS(translation_request) {
  class_<TranslationRequest>("TranslationRequest")
      .constructor(&TranslationRequestFactory, allow_raw_pointers());
}
