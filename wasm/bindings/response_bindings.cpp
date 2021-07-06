/*
 * Bindings for Response class
 *
 */

#include <emscripten/bind.h>

#include <vector>

#include "response.h"

typedef marian::bergamot::Response Response;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(byte_range) {
  value_object<marian::bergamot::ByteRange>("ByteRange")
      .field("begin", &marian::bergamot::ByteRange::begin)
      .field("end", &marian::bergamot::ByteRange::end);
}

EMSCRIPTEN_BINDINGS(response) {
  class_<Response>("Response")
      .constructor<>()
      .function("size", &Response::size)
      .function("getOriginalText", &Response::getOriginalText)
      .function("getTranslatedText", &Response::getTranslatedText)
      .function("getSourceSentence", &Response::getSourceSentenceAsByteRange)
      .function("getTranslatedSentence", &Response::getTargetSentenceAsByteRange);

  register_vector<Response>("VectorResponse");
}
