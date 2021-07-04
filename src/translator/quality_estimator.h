#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <string>
#include <vector>

#include "annotation.h"
#include "response.h"

namespace marian {
namespace bergamot {

class QualityEstimator {
 private:
  struct SentenceQualityEstimate {
    std::vector<float> wordQualitityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore;
  };
  std::vector<std::map<std::tuple<size_t, size_t>, float>> mapping;
  void initVector(std::vector<float>& emptyVector, std::string line);
  void mapBPEToWords(Response& sentence, Words words);
  std::vector<int> predictWordLevel(std::vector<std::vector<float>> feature_vector);
  std::vector<SentenceQualityEstimate> quality_scores_;
  std::vector<float> stds_, means_, coefficients_, intercept_;
  void load(AlignedMemory file_parameters);

 public:
  explicit QualityEstimator(AlignedMemory qualityEstimatorMemory);
  void computeQualityScores(Response& sentence, Words words);
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
