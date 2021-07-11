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
  const float *stds_;
  const float *means_;
  const float *coefficients_;
  const float *intercept_;
  int numFeatures_;

  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };
  struct WordFeatures {
    float bpeMean;
    float minBPE;
    float lenBPE;
    float overallMean;
  };
  struct SentenceQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    std::vector<WordFeatures> wordFeatures;
    float sentenceScore;
  };

  void load(const char *ptr_void, size_t blobSize);
  SentenceQualityEstimate mapBPEToWords(Response &sentence, Words words) const;
  AlignedVector<float> extractFeatures(SentenceQualityEstimate qualityScores) const;
  void predictWordLevel() const;

 public:
  explicit QualityEstimator(const AlignedMemory &qualityEstimatorMemory);
  void computeQualityScores(Response &sentence, Words words) const;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
