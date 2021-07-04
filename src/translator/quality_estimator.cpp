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
  size_t start = 0;
  size_t end = line.find_first_of(delimiter);

  while (end <= std::string::npos) {
    emptyVector.push_back(std::stof(line.substr(start, end - start)));
    if (end == std::string::npos) break;
    start = end + 1;
    end = line.find_first_of(delimiter, start);
  }
}

void QualityEstimator::mapBPEToWords(Response& sentence, Words words) {
  // The result is a vector of maps
  // each element of vector is a map containing the bpes of a word
  // each map key is a tuple consisting of the begining and end of the specific subword
  for (int sentenceIdx = 0; sentenceIdx < sentence.size(); sentenceIdx++) {
    auto& quality = sentence.qualityScores[sentenceIdx];
    auto eos_token = words[0].DEFAULT_EOS_ID;
    QualityEstimator::SentenceQualityEstimate sentence_quality_scores;
    sentence_quality_scores.sentenceScore = quality.sequence;
    // first_score
    sentence_quality_scores.wordByteRanges.push_back(sentence.target.wordAsByteRange(sentenceIdx, 0));
    sentence_quality_scores.wordQualitityScores.push_back(quality.word.front());
    // first_score
    size_t wordIdx = 1;
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
        num_subwords = 1;
      } else {
        auto& current_word = sentence_quality_scores.wordByteRanges.back();
        float& current_scores = sentence_quality_scores.wordQualitityScores.back();
        num_subwords += 1;
        ByteRange new_word{current_word.begin, subword.end};
        current_word = new_word;
        // incremental mean
        current_scores = current_scores + (p - current_scores) / num_subwords;
      }
    }
    this->quality_scores_.push_back(sentence_quality_scores);
  }
}

void QualityEstimator::computeQualityScores(Response& sentence, Words words) { this->mapBPEToWords(sentence, words); }

}  // namespace bergamot
}  // namespace marian
