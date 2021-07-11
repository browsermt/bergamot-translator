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
                                                                          size_t sentenceIdx, Words words) const {
  // The result is a vector of maps
  // each element of vector is a map containing the bpes of a word
  // each map key is a tuple consisting of the begining and end of the specific subword
  auto eos_token = words[0].DEFAULT_EOS_ID;
  QualityEstimator::SentenceQualityEstimate sentenceQualityScores;
  sentenceQualityScores.sentenceScore = quality.sequence;
  // first_score
  sentenceQualityScores.wordByteRanges.push_back(target.wordAsByteRange(sentenceIdx, 0));
  sentenceQualityScores.wordQualityScores.push_back(quality.word.front());
  //[0, 1, 2, 3] <- [bpe_sum_over_len, min_bpe, len_bpe, overall_mean]
  float wordFeatures[4] = {quality.word.front(), 1, quality.word.front(), (quality.sequence / quality.word.size())};
  // first_score
  int wordIdx = 1;
  int num_subwords = 1;
  for (int i = 1; i < quality.word.size(); i++) {
    auto& p = quality.word.at(i);
    if (words[wordIdx] == eos_token) {
      break;
    }
    ByteRange subword = target.wordAsByteRange(sentenceIdx, wordIdx);
    size_t subword_begin = subword.begin;
    size_t subword_end = subword.end;
    char first_word = target.text.at(subword_begin);
    wordIdx++;
    if (isspace(first_word) != 0) {
      ByteRange new_word{subword_begin + 1, subword_end};
      sentenceQualityScores.wordByteRanges.push_back(new_word);
      sentenceQualityScores.wordQualityScores.push_back(p);
      wordFeatures[0] = p;
      wordFeatures[1] = 1;
      wordFeatures[2] = p;
      num_subwords = 1;
    } else {
      auto& current_word = sentenceQualityScores.wordByteRanges.back();
      float& current_scores = sentenceQualityScores.wordQualityScores.back();
      if (wordFeatures[2] > p) {
        wordFeatures[2] = p;
      }
      num_subwords += 1;
      wordFeatures[3] = num_subwords;
      ByteRange new_word{current_word.begin, subword.end};
      current_word = new_word;
      // incremental mean
      current_scores = current_scores + (p - current_scores) / num_subwords;
      wordFeatures[0] = current_scores;
    }
  }
  return sentenceQualityScores;
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

void QualityEstimator::computeQualityScores(Quality& quality, AnnotatedText& target, size_t sentenceIdx,
                                            Words& words) const {
  auto qualityScores = mapBPEToWords(quality, target, sentenceIdx, words);
  extractFeatures(qualityScores);
  predictWordLevel();
}

}  // namespace bergamot
}  // namespace marian
