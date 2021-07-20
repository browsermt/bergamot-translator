#include "quality_estimator.h"

#include <iostream>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

constexpr int getNextMultiple(const int number, const intgemm::Index multiple) {
  const auto modWidth = number % multiple;
  return (modWidth == 0) ? number : number + multiple - modWidth;
}

constexpr intgemm::Index getIntgenWidth(const int number) {
  return getNextMultiple(number, intgemm::Int16::tile_info.a_cols);
}

constexpr intgemm::Index getIntgenBColumn(const int number) {
  return getNextMultiple(number, intgemm::Int16::tile_info.b_cols);
}

QualityEstimator::QualityEstimator(AlignedMemory&& qualityEstimatorMemory)
    : memory_(std::move(qualityEstimatorMemory)) {
  LOG(info, "[data] Loading Quality Estimator model from buffer");
  QualityEstimator::load(memory_.begin(), memory_.size());
}

void QualityEstimator::load(const char* ptr, const size_t blobSize) {
  /* File layout:
   * header
   * stds array
   * means array
   * coefficients array
   * intercepts array
   */

  ABORT_IF(blobSize < sizeof(Header), "Quality estimation file too small");
  const Header& header = *reinterpret_cast<const Header*>(ptr);
  numFeatures_ = header.numFeatures / sizeof(float);
  ptr += sizeof(Header);
  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  ABORT_IF(numFeatures_ <= 0, "The number of features cannot be equal or less than zero");
  const uint64_t expectedSize = sizeof(Header) + numFeatures_ * sizeof(float) * 4;  // stds, means, intercept, coef
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);
  const float* begin = reinterpret_cast<const float*>(ptr);
  stds_ = begin;
  means_ = (begin += numFeatures_);
  coefficients_ = (begin += numFeatures_);
  intercept_ = (begin += numFeatures_);

  modelMatrix_ = buildLinearModel();
}

std::pair<std::vector<ByteRange>, QualityEstimator::ModelFeatures> QualityEstimator::mapBPEToWords(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) const {
  const auto sentenceByteRange = target.sentenceAsByteRange(sentenceIdx);
  std::vector<ByteRange> wordByteRanges;
  WordsQualityEstimate wordsQualityEstimate;
  int bpeLen = 1;
  int wordIdx = 0;
  int subwordIdx = wordIdx;

  ModelFeatures modelFeatures;

  const auto insertNewWord = [&wordByteRanges, &modelFeatures](const ByteRange& subword, const float subwordScore,
                                                               const int lenSubwords) {
    modelFeatures.numSubWords.push_back(lenSubwords);
    modelFeatures.minWordScores.push_back(subwordScore);
    modelFeatures.wordMeanScores.push_back(subwordScore);
    wordByteRanges.push_back(subword);
  };

  const auto argumentGivenWord = [&wordByteRanges, &modelFeatures](const ByteRange& subword, const float subwordScore,
                                                                   const int lenSubwords, const int wordIndex) {
    ByteRange& currentWord = wordByteRanges.back();
    float& currentScores = modelFeatures.wordMeanScores.back();
    float& minScore = modelFeatures.minWordScores.at(wordIndex - 1);
    modelFeatures.numSubWords.at(wordIndex - 1) = lenSubwords;
    currentWord.end = subword.end;
    // incremental mean
    currentScores = currentScores + (subwordScore - currentScores) / lenSubwords;
    if (minScore > subwordScore) {
      minScore = subwordScore;
    }
  };

  float sequence = 0.0;

  for (const float& subwordScore : logProbs) {
    sequence += subwordScore;

    auto subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    if (subword.begin == sentenceByteRange.end) {  // EOS token
      insertNewWord(subword, subwordScore, 1);
      break;
    }
    char firstLetter = target.text.at(subword.begin);
    if ((isspace(firstLetter) != 0) || subwordIdx == 0) {
      if (subwordIdx != 0) {
        subword.begin += 1;  // ignore whitespace
      }
      subword.end -= 1;  // remove whitepsace
      bpeLen = 1;
      ++wordIdx;
      insertNewWord(subword, subwordScore, bpeLen);
    } else {
      bpeLen += 1;
      argumentGivenWord(subword, subwordScore, bpeLen, wordIdx);
    }
    ++subwordIdx;
  }

  modelFeatures.overallMean = sequence / subwordIdx;

  return {wordByteRanges, modelFeatures};
}

std::vector<float> QualityEstimator::predictWordScores(const AlignedVector<float>& featureMatrix,
                                                       const int numWords) const {
  float quant_mult = 1024.0f;

  AlignedVector<int16_t> A_prepared(featureMatrix.size());
  AlignedVector<int16_t> B_prepared(modelMatrix_.size());

  const auto width = getIntgenWidth(numFeatures_);
  const auto modelMatrixColumn = getIntgenBColumn(1);

  intgemm::Int16::PrepareA(featureMatrix.begin(), A_prepared.begin(), quant_mult, numWords, width);
  intgemm::Int16::PrepareB(modelMatrix_.begin(), B_prepared.begin(), quant_mult, width, modelMatrixColumn);

  AlignedVector<float> modelScores(numWords * modelMatrixColumn);

  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), numWords, width, modelMatrixColumn,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), modelScores.begin()));

  std::vector<float> wordQualityScores(numWords);

  for (int i = 0; i < numWords; ++i) {
    wordQualityScores[i] = modelScores[i * 8] + *intercept_;
  }

  return wordQualityScores;
}

AlignedVector<float> QualityEstimator::buildLinearModel() const {
  AlignedVector<float> modelMatrix(getIntgenWidth(numFeatures_) * getIntgenBColumn(1));

  for (auto& elem : modelMatrix) {
    elem = 0.0;
  }

  for (int i = 0; i < numFeatures_; ++i) {
    modelMatrix[i * 8] = *(coefficients_ + i);
  }

  return modelMatrix;
}

AlignedVector<float> QualityEstimator::extractFeatures(const ModelFeatures& modelFeatures) const {
  const intgemm::Index numWords = modelFeatures.wordMeanScores.size();

  AlignedVector<float> featureMatrix(numWords * getIntgenWidth(numFeatures_));

  for (auto& elem : featureMatrix) {
    elem = 0.0;
  }

  const int stdStart = 0;
  const int meanStart = 1 * numFeatures_;
  for (int i = 0; i < numWords; ++i) {
    int j = 0;

    for (const auto value : {modelFeatures.wordMeanScores[i], modelFeatures.minWordScores[i],
                             static_cast<float>(modelFeatures.numSubWords[i]), modelFeatures.overallMean}) {
      const auto stdsTemp = *(stds_ + j) != 0.0 ? *(stds_ + j) : 1.0f;
      featureMatrix[8 * i * numFeatures_ + j] = (value - *(means_ + j)) / stdsTemp;
      ++j;
    }
  }

  return featureMatrix;
}

float QualityEstimator::computeWordProbabilities(std::vector<float>& wordQualityScores) const {
  float sentenceScore = 0.0;

  for (int i = 0; i < wordQualityScores.size(); ++i) {
    auto& elem = wordQualityScores[i];
    elem = 1 / (1 + std::exp(-elem));
    sentenceScore += elem;
  }
  // TODO:
  // Should we include or remove the end of sentence (EOS) token?
  return sentenceScore / wordQualityScores.size();
}

QualityEstimator::WordsQualityEstimate QualityEstimator::computeQualityScores(const std::vector<float>& logProbs,
                                                                              const AnnotatedText& target,
                                                                              const size_t sentenceIdx) const {
  auto [wordByteRanges, modelFeatures] = mapBPEToWords(logProbs, target, sentenceIdx);

  const auto featureMatrix = extractFeatures(modelFeatures);
  auto wordQualityScores = predictWordScores(featureMatrix, modelFeatures.wordMeanScores.size());

  const auto setenceScore = computeWordProbabilities(wordQualityScores);

  return {wordQualityScores, wordByteRanges, setenceScore};
}
}  // namespace bergamot
}  // namespace marian
