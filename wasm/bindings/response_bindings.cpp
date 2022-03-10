/*
 * Bindings for Response class
 *
 */

#include <emscripten/bind.h>

#include <vector>

#include "response.h"

using Response = marian::bergamot::Response;
using ByteRange = marian::bergamot::ByteRange;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(byte_range) {
  value_object<ByteRange>("ByteRange").field("begin", &ByteRange::begin).field("end", &ByteRange::end);
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
