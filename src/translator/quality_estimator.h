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
  std::vector<float> modelParameters_;
  int numFeatures_ = 0;
  mutable float overallMean_  = 0.0;
  mutable std::vector<int> numSubWords_;
  mutable std::vector<float> minWordScore_;

  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };

  struct SentenceQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore = 0.0;
  };

  void load(const char *ptr_void, const size_t blobSize);
  void insertNewWord(SentenceQualityEstimate &sentenceQualityScores, const ByteRange &subword, const float subwordScore,
                     const int lenSubwords) const;
  void augumentGivenWord(SentenceQualityEstimate &sentenceQualityScores, const ByteRange &subword, const float subwordScore,
                         const int lenSubwords, const int wordIndex) const;

  AlignedVector<float> extractFeatures(const SentenceQualityEstimate& qualityScores) const;
  AlignedVector<float> buildLogisticModel() const;
  AlignedVector<float> predictWordScores(AlignedVector<float> &featureMatrix, const int numWords) const;

  SentenceQualityEstimate mapBPEToWords(const Quality& quality, const AnnotatedText& target, const size_t sentenceIdx) const;

 public:
  explicit QualityEstimator(AlignedMemory &&qualityEstimatorMemory);

  void computeQualityScores(const Quality &quality, const AnnotatedText &target, const size_t sentenceIdx) const;


};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
