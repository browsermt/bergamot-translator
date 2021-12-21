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

float getAlignmentScore(const Response& response, size_t sentenceIdx, size_t targetIdx, size_t sourceIdx) {
  return response.alignments[sentenceIdx][targetIdx][sourceIdx];
}

EMSCRIPTEN_BINDINGS(response) {
  class_<Response>("Response")
      .constructor<>()
      .function("size", &Response::size)
      .function("getQualityScores", &getQualityScores)
      .function("getOriginalText", &Response::getOriginalText)
      .function("getTranslatedText", &Response::getTranslatedText)
      .function("getSourceSentence", &Response::getSourceSentenceAsByteRange)
      .function("getTranslatedSentence", &Response::getTargetSentenceAsByteRange)
      .function("getSourceWord", &Response::getSourceWordAsByteRange)
      .function("getTranslatedWord", &Response::getTargetWordAsByteRange)
      .function("getSourceSentenceSize", &Response::getSourceSentenceSize)
      .function("getTranslatedSentenceSize", &Response::getTargetSentenceSize)
      .function("getAlignmentScore", &getAlignmentScore);

  value_object<SentenceQualityScore>("SentenceQualityScore")
      .field("wordScores", &SentenceQualityScore::wordScores)
      .field("wordByteRanges", &SentenceQualityScore::wordByteRanges)
      .field("sentenceScore", &SentenceQualityScore::sentenceScore);

  register_vector<Response>("VectorResponse");
  register_vector<SentenceQualityScore>("VectorSentenceQualityScore");
  register_vector<float>("VectorFloat");
  register_vector<ByteRange>("VectorByteRange");
}
