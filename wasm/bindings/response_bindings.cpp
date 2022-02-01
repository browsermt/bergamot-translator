/*
 * Bindings for Response class
 *
 */

#include <emscripten/bind.h>

#include <vector>

#include "response.h"

using Response = marian::bergamot::Response;
using SentenceQualityScore = marian::bergamot::Response::SentenceQualityScore;
using ByteRange = marian::bergamot::ByteRange;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(byte_range) {
  value_object<ByteRange>("ByteRange").field("begin", &ByteRange::begin).field("end", &ByteRange::end);
}

std::vector<SentenceQualityScore> getQualityScores(const Response& response) { return response.qualityScores; }

EMSCRIPTEN_BINDINGS(response) {
  class_<Response>("Response")
      .constructor<>()
      .property("ok", &Response::ok)
      .property("error", &Response::error)
      .function("size", &Response::size)
      .function("getQualityScores", &getQualityScores)
      .function("getOriginalText", &Response::getOriginalText)
      .function("getTranslatedText", &Response::getTranslatedText)
      .function("getSourceSentence", &Response::getSourceSentenceAsByteRange)
      .function("getTranslatedSentence", &Response::getTargetSentenceAsByteRange);

  value_object<SentenceQualityScore>("SentenceQualityScore")
      .field("wordScores", &SentenceQualityScore::wordScores)
      .field("wordByteRanges", &SentenceQualityScore::wordByteRanges)
      .field("sentenceScore", &SentenceQualityScore::sentenceScore);

  register_vector<Response>("VectorResponse");
  register_vector<SentenceQualityScore>("VectorSentenceQualityScore");
  register_vector<float>("VectorFloat");
  register_vector<ByteRange>("VectorByteRange");
}
