#pragma once

#include <vector>

#include "iquality_estimator.h"

namespace marian::bergamot {

/// The UnsurpervisedQE stands for Unsurpervised Quality Estimator model. It basically uses the negative log
/// probabilities (logprobs) of the translator model as proxy for quality scores. Then, for a given word, it's quality
/// score is computed by taking the mean of the negative logprobs of the tokens that make up it. The sentence score is
/// the mean of all word's neg. logprob.
class UnsupervisedQualityEstimator : public IQualityEstimator {
  friend class UnsupervisedQualityEstimatorTest;

 public:
  void computeQualityScores(Response &response, const Histories &histories) const override;

 private:
  static Response::WordsQualityEstimate computeSentenceScores(const std::vector<float> &logProbs,
                                                              const AnnotatedText &target, const size_t sentenceIdx);
};

}  // namespace marian::bergamot
