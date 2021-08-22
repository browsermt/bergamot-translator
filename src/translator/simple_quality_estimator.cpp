#include "simple_quality_estimator.h"

#include <numeric>

namespace marian::bergamot {

void SimpleQualityEstimator::computeQualityScores(Response &response, const Histories &histories) const {
  size_t sentenceIndex = 0;

  for (const auto &history : histories) {
    const auto logProbs = std::get<1>(history->top())->tracebackWordScores();
    response.qualityScores.push_back(computeQualityScores(logProbs, response.target, sentenceIndex));
    ++sentenceIndex;
  }
}

Response::WordsQualityEstimate SimpleQualityEstimator::computeQualityScores(const std::vector<float> &logProbs,
                                                                            const AnnotatedText &target,
                                                                            const size_t sentenceIdx) {
  const auto [wordBytesRanges, wordslogProbs] = remapWords(logProbs, target, sentenceIdx);

  std::vector<float> wordQualityScores;

  for (const auto &wordlogProbs : wordslogProbs) {
    wordQualityScores.push_back(std::accumulate(std::begin(wordlogProbs), std::end(wordlogProbs), float(0.0)) /
                                wordlogProbs.size());
  }

  const float sentenceScore = std::accumulate(std::begin(wordQualityScores), std::end(wordQualityScores), float(0.0)) /
                              wordQualityScores.size();

  return {wordQualityScores, wordBytesRanges, sentenceScore};
}
}  // namespace marian::bergamot
