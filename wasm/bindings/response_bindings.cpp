/*
 * Bindings for Response class
 *
 */

#include <emscripten/bind.h>

#include <vector>

#include "response.h"

using Response = marian::bergamot::Response;
using ByteRange = marian::bergamot::ByteRange;

/// Same type as Response::SentenceQualityScore, except with wordByteRanges
/// instead of wordRanges.
struct SentenceQualityScore {
  /// Quality score of each translated word
  std::vector<float> wordScores;
  /// Position of each word in the translated text
  std::vector<ByteRange> wordByteRanges;
  /// Whole sentence quality score (it is composed by the mean of its words)
  float sentenceScore = 0.0;
};

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(byte_range) {
  value_object<ByteRange>("ByteRange").field("begin", &ByteRange::begin).field("end", &ByteRange::end);
}

std::vector<SentenceQualityScore> getQualityScores(const Response& response) {
  std::vector<SentenceQualityScore> scores;
  scores.reserve(response.qualityScores.size());

  for (size_t sentenceIdx = 0; sentenceIdx < response.qualityScores.size(); ++sentenceIdx) {
    std::vector<ByteRange> wordByteRanges;
    wordByteRanges.reserve(response.qualityScores[sentenceIdx].wordRanges.size());

    for (auto&& word : response.qualityScores[sentenceIdx].wordRanges) {
      wordByteRanges.emplace_back();
      wordByteRanges.back().begin = response.target.wordAsByteRange(sentenceIdx, word.begin).begin;
      wordByteRanges.back().end = response.target.wordAsByteRange(sentenceIdx, word.end).begin;
    }

    scores.emplace_back(SentenceQualityScore{response.qualityScores[sentenceIdx].wordScores, std::move(wordByteRanges),
                                             response.qualityScores[sentenceIdx].sentenceScore});
  }

  return scores;
}

EMSCRIPTEN_BINDINGS(response) {
  class_<Response>("Response")
      .constructor<>()
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
