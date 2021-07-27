#include "quality_estimator.h"

#include <iostream>

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

QualityEstimator::LogisticRegressor::LogisticRegressor(Scale&& scaleParam, IntgemmMatrix&& coefficientsParam,
                                                       const size_t numCoefficientsParam, const float interceptParam)
    : scale(std::move(scaleParam)),
      coefficients(std::move(coefficientsParam)),
      numCoefficients(numCoefficientsParam),
      intercept(interceptParam) {}

QualityEstimator::QualityEstimator(const AlignedMemory& qualityEstimatorMemory)
    : logisticRegressor_(QualityEstimator::fromAlignedMemory(qualityEstimatorMemory)) {}

QualityEstimator::LogisticRegressor QualityEstimator::fromAlignedMemory(const AlignedMemory& qualityEstimatorMemory) {
  LOG(info, "[data] Loading Quality Estimator model from buffer");
  /* File layout:
   * header
   * stds array
   * means array
   * coefficients array
   * intercept
   */

  const char* ptr = qualityEstimatorMemory.begin();
  const size_t blobSize = qualityEstimatorMemory.size();

  ABORT_IF(blobSize < sizeof(Header), "Quality estimation file too small");
  const Header& header = *reinterpret_cast<const Header*>(ptr);

  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  ABORT_IF(header.lrParametersDims <= 0, "The number of lr parameter dimension cannot be equal or less than zero");

  const size_t numLrParamsWithDimension = 3;  // stds, means and coefficients
  const size_t numIntercept = 1;

  const uint64_t expectedSize =
      sizeof(Header) + (numLrParamsWithDimension * header.lrParametersDims + numIntercept) * sizeof(float);
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);

  ptr += sizeof(Header);
  const float* memoryIndex = reinterpret_cast<const float*>(ptr);

  const float* stds = memoryIndex;
  const float* means = memoryIndex += header.lrParametersDims;
  const float* coefficients = memoryIndex += header.lrParametersDims;
  const float intercept = *(memoryIndex += header.lrParametersDims);

  Scale scale;
  scale.means.resize(header.lrParametersDims);
  scale.stds.resize(header.lrParametersDims);

  const size_t coefficientsColumnSize = 1;

  IntgemmMatrix coefficientsMatrix(header.lrParametersDims, coefficientsColumnSize, intgemm::Int16::tile_info.b_rows,
                                   intgemm::Int16::tile_info.b_cols);

  for (int i = 0; i < header.lrParametersDims; ++i) {
    scale.stds[i] = *(stds + i);
    scale.means[i] = *(means + i);
    coefficientsMatrix.data[i * coefficientsMatrix.cols] = *(coefficients + i);
  }

  return LogisticRegressor(std::move(scale), std::move(coefficientsMatrix), header.lrParametersDims, intercept);
}

AlignedMemory QualityEstimator::toAlignedMemory() const {
  const size_t lrParametersDims = logisticRegressor_.scale.means.size();

  const size_t lrSize = (logisticRegressor_.scale.means.size() + logisticRegressor_.scale.stds.size() +
                         logisticRegressor_.numCoefficients) *
                            sizeof(float) +
                        sizeof(logisticRegressor_.intercept);

  QualityEstimator::Header header = {BINARY_QE_MODEL_MAGIC, lrParametersDims};
  marian::bergamot::AlignedMemory memory(sizeof(header) + lrSize);

  char* buffer = memory.begin();

  memcpy(buffer, &header, sizeof(header));
  buffer += sizeof(header);

  for (const float std : logisticRegressor_.scale.stds) {
    memcpy(buffer, &std, sizeof(std));
    buffer += sizeof(std);
  }

  for (const float mean : logisticRegressor_.scale.means) {
    memcpy(buffer, &mean, sizeof(mean));
    buffer += sizeof(mean);
  }

  for (size_t i = 0; i < lrParametersDims; ++i) {
    const float coefficient = logisticRegressor_.coefficients.data[i * logisticRegressor_.coefficients.cols];
    memcpy(buffer, &coefficient, sizeof(coefficient));
    buffer += sizeof(coefficient);
  }

  memcpy(buffer, &logisticRegressor_.intercept, sizeof(logisticRegressor_.intercept));
  buffer += sizeof(logisticRegressor_.intercept);

  return memory;
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
  AlignedVector<int16_t> B_prepared(coefficients.data.size());

  intgemm::Int16::PrepareA(features.data.begin(), A_prepared.begin(), quant_mult, features.rows, features.cols);
  intgemm::Int16::PrepareB(coefficients.data.begin(), B_prepared.begin(), quant_mult, coefficients.rows,
                           coefficients.cols);

  AlignedVector<float> modelScores(features.rows * coefficients.cols);

  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), features.rows, features.cols, coefficients.cols,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), modelScores.begin()));

  std::vector<float> wordQualityScores(features.rows);

  for (int i = 0; i < features.rows; ++i) {
    wordQualityScores[i] = modelScores[i * coefficients.cols] + intercept;
  }

  return wordQualityScores;
}

QualityEstimator::IntgemmMatrix QualityEstimator::LogisticRegressor::transformFeatures(
    const std::vector<WordFeatures>& wordsFeatures, const float overallMean) const {
  const intgemm::Index numWords = wordsFeatures.size();

  const size_t numFeatures = 4;

  QualityEstimator::IntgemmMatrix features(numWords, numFeatures, intgemm::Int16::tile_info.a_rows,
                                           intgemm::Int16::tile_info.a_cols);

  const auto getStds = [](const auto stds) { return stds != 0.0 ? stds : 1.0f; };

  for (int i = 0; i < numWords; ++i) {
    const WordFeatures& wordFeatures = wordsFeatures[i];
    features.data[features.cols * i + 0] = (wordFeatures.meanScore - scale.means[0]) / getStds(scale.stds[0]);
    features.data[features.cols * i + 1] = (wordFeatures.minScore - scale.means[1]) / getStds(scale.stds[1]);
    features.data[features.cols * i + 2] = (wordFeatures.numSubWords - scale.means[2]) / getStds(scale.stds[2]);
    features.data[features.cols * i + 3] = (overallMean - scale.means[3]) / getStds(scale.stds[3]);
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
