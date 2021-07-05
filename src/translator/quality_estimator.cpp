#include "quality_estimator.h"

#include <iostream>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

QualityEstimator::QualityEstimator(AlignedMemory qualityEstimatorMemory) {
  // QE coefficients memory passed as bytes.
  QualityEstimator::load(std::move(qualityEstimatorMemory));
}

void QualityEstimator::load(AlignedMemory file_parameters) {
  char delimiter = '\n';
  int line_position = 0;
  std::string line(file_parameters.begin());
  std::vector<std::string> file_input;
  file_input.reserve(4);
  std::istringstream istr(line);
  while (std::getline(istr, line, delimiter)) {
    file_input.emplace_back(line);
  }
  ABORT_IF(file_input.size() != 4, "Model file should contains 4 lines, one per model parameter");
  QualityEstimator::initVector(this->stds_, file_input[0]);
  QualityEstimator::initVector(this->means_, file_input[1]);
  QualityEstimator::initVector(this->coefficients_, file_input[2]);
  QualityEstimator::initVector(this->intercept_, file_input[3]);
}

void QualityEstimator::initVector(std::vector<float>& emptyVector, std::string line) {
  char delimiter = ' ';
  std::istringstream istr(line);
  while (std::getline(istr, line, delimiter)) {
    emptyVector.push_back(std::stof(line));
  }
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
