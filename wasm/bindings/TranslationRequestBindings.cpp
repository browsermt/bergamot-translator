/*
 * Bindings for TranslationRequest class
 *
 */

#include <emscripten/bind.h>

#include "response_options.h"

typedef marian::bergamot::ResponseOptions TranslationRequest;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_request) {
  class_<TranslationRequest>("TranslationRequest")
    .constructor<>()
    ;
}
