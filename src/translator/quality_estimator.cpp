#include "quality_estimator.h"

#include <iostream>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

QualityEstimator::QualityEstimator(const AlignedMemory& qualityEstimatorMemory) {
  LOG(info, "[data] Loading Quality Estimator model from buffer");
  QualityEstimator::load(qualityEstimatorMemory.begin(), qualityEstimatorMemory.size());
}

void QualityEstimator::load(const char* ptr, size_t blobSize) {
  /* File layout:
   * header
   * stds array
   * means array
   * coefficients array
   * intercepts array
   */
  ABORT_IF(blobSize < HEADER_SIZE, "Quality estimation file too small");
  const Header& header = *reinterpret_cast<const Header*>(ptr);
  numFeatures_ = header.numFeatures / sizeof(float);
  ptr += sizeof(Header);
  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  uint64_t expectedSize = sizeof(Header) + numFeatures_ * sizeof(float) * 4;  // stds, means, intercept, coef
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);
  const float* begin = reinterpret_cast<const float*>(ptr);
  stds_ = begin;
  means_ = (begin += numFeatures_);
  coefficients_ = (begin += numFeatures_);
  intercept_ = (begin += numFeatures_);
}

QualityEstimator::SentenceQualityEstimate QualityEstimator::mapBPEToWords(Quality quality, AnnotatedText target,
                                                                          size_t sentenceIdx) const {
  const ByteRange& sentenceByteRange = target.sentenceAsByteRange(sentenceIdx);
  QualityEstimator::SentenceQualityEstimate sentenceQualityScores;
  int bpeLen = 1;
  int wordIdx = 0;
  int subwordIdx = wordIdx;
  for (float& subwordScore : quality.word) {
    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);
    if (subword.begin == sentenceByteRange.end) {  // EOS token
      insertNewWord(sentenceQualityScores, subword, subwordScore, 1);
      break;
    }
    char firstLetter = target.text.at(subword.begin);
    if ((isspace(firstLetter) != 0) || subwordIdx == 0) {
      if (subwordIdx != 0) {
        subword.begin += 1;  // ignore whitespace
      }
      subword.end -= 1;  // remove whitepsace
      bpeLen = 1;
      wordIdx++;
      insertNewWord(sentenceQualityScores, subword, subwordScore, bpeLen);
    } else {
      bpeLen += 1;
      augumentGivenWord(sentenceQualityScores, subword, subwordScore, bpeLen, wordIdx);
    }
    subwordIdx++;
  }
  return sentenceQualityScores;
}

void QualityEstimator::insertNewWord(QualityEstimator::SentenceQualityEstimate& sentenceQualityScores,
                                     ByteRange& subword, float& subwordScore, int lenSubwords) const {
  numSubWords_.push_back(lenSubwords);
  minWordScore_.push_back(subwordScore);
  sentenceQualityScores.wordByteRanges.push_back(subword);
  sentenceQualityScores.wordQualityScores.push_back(subwordScore);
}

void QualityEstimator::augumentGivenWord(SentenceQualityEstimate& sentenceQualityScores, ByteRange& subword,
                                         float& subwordScore, int lenSubwords, int wordIndex) const {
  ByteRange& currentWord = sentenceQualityScores.wordByteRanges.back();
  float& currentScores = sentenceQualityScores.wordQualityScores.back();
  float& minScore = minWordScore_.at(wordIndex - 1);
  numSubWords_.at(wordIndex - 1) = lenSubwords;
  currentWord.end = subword.end;
  // incremental mean
  currentScores = currentScores + (subwordScore - currentScores) / lenSubwords;
  if (minScore > subwordScore) {
    minScore = subwordScore;
  }
}

void QualityEstimator::predictWordLevel() const {}

AlignedVector<float> QualityEstimator::extractFeatures(QualityEstimator::SentenceQualityEstimate qualityScores) const {
  // auto& word_scores = qualityScores.wordFeatures;  // there will always be only one sentence
  // const intgemm::Index num_words = word_scores.size();
  // const intgemm::Index width = 16;
  AlignedVector<float> feature_matrix;
  // int j = 0;
  // for (auto& it : word_scores) {
  //   feature_matrix[j] = (it.bpeMean - means_[0]) / stds_[0];
  //   feature_matrix[j + 1] = (it.lenBPE - means_[1]) / stds_[1];
  //   feature_matrix[j + 2] = (it.minBPE - means_[2]) / stds_[2];
  //   feature_matrix[j + 3] = (it.overallMean - means_[3]) / stds_[3];
  //   j += numFeatures_;
  // }
  return feature_matrix;
}

void QualityEstimator::computeQualityScores(Quality& quality, AnnotatedText& target, size_t sentenceIdx) const {
  auto qualityScores = mapBPEToWords(quality, target, sentenceIdx);
  extractFeatures(qualityScores);
  predictWordLevel();
}

}  // namespace bergamot
}  // namespace marian
