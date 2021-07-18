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
// ASCII and Unicode text files never start with the following 64 bits
constexpr std::size_t BINARY_QE_MODEL_MAGIC = 0x78cc336f1d54b180;
constexpr std::size_t HEADER_SIZE = 64 /* Keep alignment by being a multiple of 64 bytes */;

class QualityEstimator {
 private:
  AlignedMemory memory_;
  const float *stds_ = nullptr;
  const float *means_ = nullptr;
  const float *coefficients_ = nullptr;
  const float *intercept_ = nullptr;
  int numFeatures_ = 0;
  AlignedVector<float> modelMatrix_;

  void load(const char *ptr_void, const size_t blobSize);

 public:
  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };

  struct WordsQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore = 0.0;
  };

  struct ModelFeatures {
    std::vector<float> wordMeanScores;
    std::vector<float> minWordScores;
    std::vector<int> numSubWords;
    float overallMean = 0.0;
  };

  explicit QualityEstimator(AlignedMemory &&qualityEstimatorMemory);

  std::pair<std::vector<ByteRange>, ModelFeatures> mapBPEToWords(const std::vector< float > &logProbs, const AnnotatedText &target,
                                                                 const size_t sentenceIdx) const;
  std::vector<float> predictWordScores(const AlignedVector<float> &featureMatrix, const int numWords) const;
  AlignedVector<float> buildLogisticModel() const;
  AlignedVector<float> extractFeatures(const ModelFeatures &modelFeatures) const;
  float computeWordProbabilities(std::vector<float> &wordQualityScores) const;

  WordsQualityEstimate computeQualityScores(const std::vector< float > &logProbs, const AnnotatedText &target,
                                            const size_t sentenceIdx) const;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
