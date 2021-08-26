#pragma once

#include <vector>

#include "annotation.h"
#include "response.h"
#include "translator/history.h"

namespace marian::bergamot {

/// Interface for quality estimator
class IQualityEstimator {
 public:
  // Computes quality-scores using values from History and precomputed
  // and stored tokenizations within Response.
  //
  // @param [inout] response: Partially constructed response, holding tokenization info
  // @param [in] histories: Histories obtained from translating a blob of source-text
  // for source and target. The quality-scores for each sentence obtained from source-text blob
  // are written out as WordsQualityEstimate into response.
  virtual void computeQualityScores(Response &response, const Histories &histories) const = 0;
};

std::pair<std::vector<ByteRange>, std::vector<std::vector<float> > > remapWordsAndLogProbs(
    const std::vector<float> &logProbs, const AnnotatedText &target, const size_t sentenceIdx);
}  // namespace marian::bergamot
