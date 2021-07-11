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
  const int offset = header.numFeatures / sizeof(float);
  ptr += sizeof(Header);
  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  uint64_t expectedSize = sizeof(Header) + offset * sizeof(float) * 4;  // stds, means, intercept, coef
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);
  const float* begin = reinterpret_cast<const float*>(ptr);
  stds_ = begin;
  means_ = (begin += offset);
  coefficients_ = (begin += offset);
  intercept_ = (begin += offset);
}

void QualityEstimator::mapBPEToWords(Response& sentence, Words words) {
  // The result is a vector of maps
  // each element of vector is a map containing the bpes of a word
  // each map key is a tuple consisting of the begining and end of the specific subword
  for (int sentenceIdx = 0; sentenceIdx < sentence.size(); sentenceIdx++) {
    WordFeatures features;
    auto& quality = sentence.qualityScores[sentenceIdx];
    auto eos_token = words[0].DEFAULT_EOS_ID;
    QualityEstimator::SentenceQualityEstimate sentence_quality_scores;
    sentence_quality_scores.sentenceScore = quality.sequence;
    // first_score
    sentence_quality_scores.wordByteRanges.push_back(sentence.target.wordAsByteRange(sentenceIdx, 0));
    sentence_quality_scores.wordQualitityScores.push_back(quality.word.front());
    features.min_bpe = quality.word.front();
    features.len_bpe = 1;
    features.bpe_sum_over_len = quality.word.front();
    features.overall_mean = quality.sequence / quality.word.size();
    sentence_quality_scores.wordFeatures.push_back(features);
    // first_score
    int wordIdx = 1;
    int num_subwords = 1;
    for (int i = 1; i < quality.word.size(); i++) {
      auto& p = quality.word.at(i);
      if (words[wordIdx] == eos_token) {
        break;
      }
      ByteRange subword = sentence.target.wordAsByteRange(sentenceIdx, wordIdx);
      size_t subword_begin = subword.begin;
      size_t subword_end = subword.end;
      char first_word = sentence.target.text.at(subword_begin);
      wordIdx++;
      if (isspace(first_word) != 0) {
        ByteRange new_word{subword_begin + 1, subword_end};
        sentence_quality_scores.wordByteRanges.push_back(new_word);
        sentence_quality_scores.wordQualitityScores.push_back(p);
        features.min_bpe = p;
        features.len_bpe = 1;
        features.bpe_sum_over_len = p;
        features.overall_mean = quality.sequence / quality.word.size();
        sentence_quality_scores.wordFeatures.push_back(features);
        num_subwords = 1;
      } else {
        auto& current_word = sentence_quality_scores.wordByteRanges.back();
        float& current_scores = sentence_quality_scores.wordQualitityScores.back();
        auto& current_features = sentence_quality_scores.wordFeatures.back();
        if (current_features.min_bpe > p) {
          current_features.min_bpe = p;
        }
        num_subwords += 1;
        current_features.len_bpe = num_subwords;
        ByteRange new_word{current_word.begin, subword.end};
        current_word = new_word;
        // incremental mean
        current_scores = current_scores + (p - current_scores) / num_subwords;
        current_features.bpe_sum_over_len = current_scores;
      }
    }
    this->quality_scores_.push_back(sentence_quality_scores);
  }
}

void QualityEstimator::predictWordLevel() {}

AlignedVector<float> QualityEstimator::extractFeatutes() {
  auto& word_scores = this->quality_scores_[0].wordFeatures;  // there will always be only one sentence
  const intgemm::Index num_words = word_scores.size();
  const intgemm::Index width = 16;
  AlignedVector<float> feature_matrix(num_words * width);
  int j = 0;
  for (auto& it : word_scores) {
    feature_matrix[j] = (it.bpe_sum_over_len - this->means_[0]) / this->stds_[0];
    feature_matrix[j + 1] = (it.len_bpe - this->means_[1]) / this->stds_[1];
    feature_matrix[j + 2] = (it.min_bpe - this->means_[2]) / this->stds_[2];
    feature_matrix[j + 3] = (it.overall_mean - this->means_[3]) / this->stds_[3];
    j += 4;
  }
  return feature_matrix;
}

void QualityEstimator::computeQualityScores(Response& sentence, Words words) {
  this->mapBPEToWords(sentence, words);
  this->extractFeatutes();
  this->predictWordLevel();
}

}  // namespace bergamot
}  // namespace marian
