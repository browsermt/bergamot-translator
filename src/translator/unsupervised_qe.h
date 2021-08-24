#pragma once

#include <vector>

#include "iquality_estimator.h"

namespace marian::bergamot {

/// "Unsupervised" approach quality model
/// It's only return the mean of BPE tokens of a given word already compute by marian
class UnsupervisedQE : public IQualityEstimator {
   friend class UnsupervisedQETest;
 public:
  UnsupervisedQE() = default;
  ~UnsupervisedQE() = default;

  void computeQualityScores(Response &response, const Histories &histories) const override;

 private:
  static Response::WordsQualityEstimate computeSentenceScores(const std::vector<float> &logProbs,
                                                              const AnnotatedText &target, const size_t sentenceIdx);
};

}  // namespace marian::bergamot
