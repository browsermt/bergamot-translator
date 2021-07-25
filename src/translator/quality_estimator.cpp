#include "quality_estimator.h"

#include <iostream>
#include <iterator>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

constexpr intgemm::Index getNextMultiple(const intgemm::Index number, const intgemm::Index multiple) {
  const intgemm::Index modWidth = number % multiple;
  return (modWidth == 0) ? number : number + multiple - modWidth;
}

QualityEstimator::IntgemmMatrix::IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index colsParam,
                                               const intgemm::Index rowsMultiplier, const intgemm::Index colsMultiplier)
    : rows(getNextMultiple(rowsParam, rowsMultiplier)),
      cols(getNextMultiple(colsParam, colsMultiplier)),
      data(rows * cols) {
  for (float& elem : data) {
    elem = 0.0;
  }
}

QualityEstimator::LogisticRegressor::LogisticRegressor(Scale&& scale, IntgemmMatrix&& coefficients,
                                                       const float intercept)
    : scale_(std::move(scale)), coefficients_(std::move(coefficients)), intercept_(intercept) {}

QualityEstimator::QualityEstimator(const AlignedMemory& qualityEstimatorMemory)
    : logisticRegressor_(QualityEstimator::load(qualityEstimatorMemory)) {}

QualityEstimator::LogisticRegressor QualityEstimator::load(const AlignedMemory& qualityEstimatorMemory) {
  LOG(info, "[data] Loading Quality Estimator model from buffer");
  /* File layout:
   * header
   * stds array
   * means array
   * coefficients array
   * intercepts array
   */

  const char* ptr = qualityEstimatorMemory.begin();
  const size_t blobSize = qualityEstimatorMemory.size();

  ABORT_IF(blobSize < sizeof(Header), "Quality estimation file too small");
  const Header& header = *reinterpret_cast<const Header*>(ptr);
  const uint64_t numLRParams = header.numFeatures / sizeof(float);
  ptr += sizeof(Header);
  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  ABORT_IF(numLRParams <= 0, "The number of features cannot be equal or less than zero");

  const uint64_t expectedSize =
      sizeof(Header) + numLRParams * sizeof(float) * LogisticRegressor::ParametersDims;  // stds, means, intercept, coef
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);

  const float* memoryIndex = reinterpret_cast<const float*>(ptr);

  const float* stds = memoryIndex;
  const float* means = memoryIndex += LogisticRegressor::ParametersDims;
  const float* coefficients = memoryIndex += LogisticRegressor::ParametersDims;
  const float intercept = *(memoryIndex += LogisticRegressor::ParametersDims);

  Scale scale;
  scale.means.resize(LogisticRegressor::ParametersDims);
  scale.stds.resize(LogisticRegressor::ParametersDims);

  IntgemmMatrix coefficientsMatrix(LogisticRegressor::ParametersDims, LogisticRegressor::CoefficientsColumnSize,
                                   intgemm::Int16::tile_info.b_rows, intgemm::Int16::tile_info.b_cols);

  for (int i = 0; i < LogisticRegressor::ParametersDims; ++i) {
    scale.stds[i] = *(stds + i);
    scale.means[i] = *(means + i);
    coefficientsMatrix.data[i * coefficientsMatrix.cols] = *(coefficients + i);
  }

  return LogisticRegressor(std::move(scale), std::move(coefficientsMatrix), intercept);
}

std::tuple<std::vector<ByteRange>, std::vector<QualityEstimator::WordFeatures>, float> QualityEstimator::mapBPEToWords(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) const {
  if (logProbs.empty() || target.text.empty()) {
    return {{}, {}, 0.0};
  }

  std::vector<ByteRange> wordByteRanges;
  std::vector<WordFeatures> wordsFeatures;

  // The first subword it's always a beginning of a word
  float subwordScore = logProbs[0];
  ByteRange subword = target.wordAsByteRange(sentenceIdx, 0);

  float sequence = subwordScore;

  wordsFeatures.push_back({1, subwordScore, subwordScore});
  wordByteRanges.push_back(subword);

  size_t subwordIdx = 1;
  ///  A word is composed of multiple subtokens. The definition of an "entire"
  /// word is the presence of whitespace. The QE model ignores the presence
  /// of the EOS token, and hence we only need to iterate n-1 positions.
  for (; subwordIdx < (logProbs.size() - 1); ++subwordIdx) {
    const float subwordScore = logProbs[subwordIdx];
    sequence += subwordScore;

    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    const char firstLetter = target.text.at(subword.begin);

    // if the first character is whitespace, it's a beginning of a new word
    if (isspace(firstLetter)) {
      ++subword.begin;
      wordsFeatures.push_back({1, subwordScore, subwordScore});
      wordByteRanges.push_back(subword);
    } else {
      // update last word byte range and word features

      ByteRange& currentWord = wordByteRanges.back();
      WordFeatures& currentWordFeatures = wordsFeatures.back();

      currentWord.end = subword.end;
      ++currentWordFeatures.numSubWords;
      // incremental mean
      currentWordFeatures.meanScore += (subwordScore - currentWordFeatures.meanScore) / currentWordFeatures.numSubWords;

      if (currentWordFeatures.minScore > subwordScore) {
        currentWordFeatures.minScore = subwordScore;
      }
    }
  }

  const float overallMean = sequence / subwordIdx;

  return {wordByteRanges, wordsFeatures, overallMean};
}

std::vector<float> QualityEstimator::LogisticRegressor::vectorResult(const IntgemmMatrix& features) const {
  float quant_mult = 1024.0f;

  AlignedVector<int16_t> A_prepared(features.data.size());
  AlignedVector<int16_t> B_prepared(coefficients_.data.size());

  intgemm::Int16::PrepareA(features.data.begin(), A_prepared.begin(), quant_mult, features.rows, features.cols);
  intgemm::Int16::PrepareB(coefficients_.data.begin(), B_prepared.begin(), quant_mult, coefficients_.rows,
                           coefficients_.cols);

  AlignedVector<float> modelScores(features.rows * coefficients_.cols);

  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), features.rows, features.cols, coefficients_.cols,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), modelScores.begin()));

  std::vector<float> wordQualityScores(features.rows);

  for (int i = 0; i < features.rows; ++i) {
    wordQualityScores[i] = modelScores[i * coefficients_.cols] + intercept_;
  }

  return wordQualityScores;
}

QualityEstimator::IntgemmMatrix QualityEstimator::LogisticRegressor::transformFeatures(
    const std::vector<WordFeatures>& wordsFeatures, const float overallMean) const {
  const intgemm::Index numWords = wordsFeatures.size();

  QualityEstimator::IntgemmMatrix features(numWords, NumberOfFeatures, intgemm::Int16::tile_info.a_rows,
                                           intgemm::Int16::tile_info.a_cols);

  const auto getStds = [](const auto stds) { return stds != 0.0 ? stds : 1.0f; };

  for (int i = 0; i < numWords; ++i) {
    const WordFeatures& wordFeatures = wordsFeatures[i];
    features.data[features.cols * i + 0] = (wordFeatures.meanScore - scale_.means[0]) / getStds(scale_.stds[0]);
    features.data[features.cols * i + 1] = (wordFeatures.minScore - scale_.means[1]) / getStds(scale_.stds[1]);
    features.data[features.cols * i + 2] = (wordFeatures.numSubWords - scale_.means[2]) / getStds(scale_.stds[2]);
    features.data[features.cols * i + 3] = (overallMean - scale_.means[3]) / getStds(scale_.stds[3]);
  }

  return features;
}

float QualityEstimator::LogisticRegressor::resultToProbabilities(std::vector<float>& wordQualityScores) const {
  float sentenceScore = 0.0;

  for (float& wordScore : wordQualityScores) {
    wordScore = 1 / (1 + std::exp(-wordScore));
    sentenceScore += wordScore;
  }

  return sentenceScore / wordQualityScores.size();
}

QualityEstimator::WordsQualityEstimate QualityEstimator::LogisticRegressor::predictQualityScores(
    const std::vector<ByteRange>& wordByteRanges, const std::vector<WordFeatures>& wordsFeatures,
    const float overallMean) const {
  std::vector<float> wordQualityScores = vectorResult(transformFeatures(wordsFeatures, overallMean));
  const float sentenceScore = resultToProbabilities(wordQualityScores);
  return {wordQualityScores, wordByteRanges, sentenceScore};
}

QualityEstimator::WordsQualityEstimate QualityEstimator::computeQualityScores(const std::vector<float>& logProbs,
                                                                              const AnnotatedText& target,
                                                                              const size_t sentenceIdx) const {
  const auto [wordByteRanges, wordsFeatures, overallMean] = mapBPEToWords(logProbs, target, sentenceIdx);

  return logisticRegressor_.predictQualityScores(wordByteRanges, wordsFeatures, overallMean);
}
}  // namespace bergamot
}  // namespace marian
