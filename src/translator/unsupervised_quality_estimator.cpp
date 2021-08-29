#include "unsupervised_quality_estimator.h"

#include <numeric>

namespace marian::bergamot {

void UnsupervisedQualityEstimator::computeQualityScores(const Histories &histories, Response &response) const {
  size_t sentenceIndex = 0;

  for (const auto &history : histories) {
    const auto logProbs = std::get<1>(history->top())->tracebackWordScores();
    response.qualityScores.push_back(computeSentenceScores(logProbs, response.target, sentenceIndex));
    ++sentenceIndex;
  }
}

Response::WordsQualityEstimate UnsupervisedQualityEstimator::computeSentenceScores(const std::vector<float> &logProbs,
                                                                                   const AnnotatedText &target,
                                                                                   const size_t sentenceIdx) {
  const auto [subwordByWordBytesRanges, wordlogProbs] = remapWordsAndLogProbs(logProbs, target, sentenceIdx);

  std::vector<float> wordQualityScores;

  for (const auto &wordlogProb : wordlogProbs) {
    wordQualityScores.push_back(std::accumulate(std::begin(wordlogProb), std::end(wordlogProb), float(0.0)) /
                                wordlogProb.size());
  }

  const float sentenceScore = std::accumulate(std::begin(wordQualityScores), std::end(wordQualityScores), float(0.0)) /
                              wordQualityScores.size();

  std::vector<ByteRange> wordBytesRanges;

  for (const auto &subwords : subwordByWordBytesRanges) {
    if (!subwords.empty()) {
      wordBytesRanges.emplace_back(ByteRange{subwords.begin()->begin, subwords.rbegin()->end});
    }
  }

  return {wordQualityScores, wordBytesRanges, sentenceScore};
}
}  // namespace marian::bergamot
