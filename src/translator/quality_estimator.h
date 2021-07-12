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
  std::vector<float> modelParameters_;
  int numFeatures_;
  mutable float overallMean_;
  mutable std::vector<int> numSubWords_;
  mutable std::vector<float> minWordScore_;

  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };
  struct SentenceQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore;
  };

  void load(const char *ptr_void, size_t blobSize);
  SentenceQualityEstimate mapBPEToWords(Quality quality, AnnotatedText target, size_t sentenceIdx) const;
  void insertNewWord(SentenceQualityEstimate &sentenceQualityScores, ByteRange &subword, float &subwordScore,
                     int lenSubwords) const;
  void augumentGivenWord(SentenceQualityEstimate &sentenceQualityScores, ByteRange &subword, float &subwordScore,
                         int lenSubwords, int wordIndex) const;
  AlignedVector<float> extractFeatures(SentenceQualityEstimate qualityScores) const;
  AlignedVector<float> buildLogisticModel() const;
  AlignedVector<float> predictWordScores(AlignedVector<float> &featureMatrix, int numWords) const;

 public:
  explicit QualityEstimator(const AlignedMemory &qualityEstimatorMemory);
  void computeQualityScores(Quality &quality, AnnotatedText &target, size_t sentenceIdx) const;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
