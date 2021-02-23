/*
 * Bindings for TranslationRequest class
 *
 */

#include <emscripten/bind.h>

#include "TranslationRequest.h"

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_request) {
  class_<TranslationRequest>("TranslationRequest")
    .constructor<>()
    ;
}
