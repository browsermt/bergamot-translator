#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <string>
#include <vector>

#include "annotation.h"
#include "intgemm/intgemm.h"
#include "response.h"

namespace marian {
namespace bergamot {

class QualityEstimator {
 private:
  struct WordFeatures {
    float bpe_sum_over_len;
    float min_bpe;
    float len_bpe;
    float overall_mean;
  };
  struct SentenceQualityEstimate {
    std::vector<float> wordQualitityScores;
    std::vector<ByteRange> wordByteRanges;
    std::vector<WordFeatures> wordFeatures;
    float sentenceScore;
  };
  void initVector(std::vector<float>& emptyVector, std::string line);
  void load(AlignedMemory file_parameters);
  std::vector<SentenceQualityEstimate> quality_scores_;
  std::vector<float> stds_, means_;
  std::vector<float> coefficients_, intercept_;

  void mapBPEToWords(Response& sentence, Words words);
  AlignedVector<float> extractFeatutes();
  void predictWordLevel();

 public:
  explicit QualityEstimator(AlignedMemory qualityEstimatorMemory);
  void computeQualityScores(Response& sentence, Words words);
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
