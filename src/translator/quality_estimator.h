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

  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };
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
  std::vector<SentenceQualityEstimate> quality_scores_;

  void load(const char *ptr_void, size_t blobSize);
  void mapBPEToWords(Response &sentence, Words words);
  AlignedVector<float> extractFeatutes();
  void predictWordLevel();

 public:
  explicit QualityEstimator(const AlignedMemory &qualityEstimatorMemory);
  void computeQualityScores(Response &sentence, Words words);
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
