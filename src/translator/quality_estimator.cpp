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

  IntgemmMatrix coefficientsMatrix(LogisticRegressor::ParametersDims, LogisticRegressor::CoefficientsColumn, intgemm::Int16::tile_info.b_rows,
                                   intgemm::Int16::tile_info.b_cols);

  for (int i = 0; i < LogisticRegressor::ParametersDims; ++i) {
    scale.stds[i] = *(stds + i);
    scale.means[i] = *(means + i);
    coefficientsMatrix.data[i * coefficientsMatrix.cols] = *(coefficients + i);
  }

  return LogisticRegressor(std::move(scale), std::move(coefficientsMatrix), intercept);
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

    auto wordText = target.word(sentenceIdx, subwordIdx);
    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);
    /// a word is composed by multiple subtokens. The EOS token
    /// is the only one where subword.begin is the same as the end
    /// Therefore, if both are equal, we have reached the end of sentence
    /// If we do not apply an break statement the code would fail since
    /// target.text do not contain the EOS token
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

// std::pair<std::vector<ByteRange>, std::vector<QualityEstimator::WordFeatures>> QualityEstimator::mapBPEToWords2(
//     const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) const {
//   const auto sentenceByteRange = target.sentenceAsByteRange(sentenceIdx);
//   std::vector<ByteRange> wordByteRanges;
//   WordsQualityEstimate wordsQualityEstimate;
//   int bpeLen = 1;
//   int wordIdx = 0;
//   int subwordIdx = wordIdx;

//   ModelFeatures modelFeatures;

//   const auto insertNewWord = [&wordByteRanges, &modelFeatures](const ByteRange& subword, const float subwordScore,
//                                                                const int lenSubwords) {
//     modelFeatures.numSubWords.push_back(lenSubwords);
//     modelFeatures.minWordScores.push_back(subwordScore);
//     modelFeatures.wordMeanScores.push_back(subwordScore);
//     wordByteRanges.push_back(subword);
//   };

//   const auto argumentGivenWord = [&wordByteRanges, &modelFeatures](const ByteRange& subword, const float subwordScore,
//                                                                    const int lenSubwords, const int wordIndex) {
//     ByteRange& currentWord = wordByteRanges.back();
//     float& currentScores = modelFeatures.wordMeanScores.back();
//     float& minScore = modelFeatures.minWordScores.at(wordIndex - 1);
//     modelFeatures.numSubWords.at(wordIndex - 1) = lenSubwords;
//     currentWord.end = subword.end;
//     // incremental mean
//     currentScores = currentScores + (subwordScore - currentScores) / lenSubwords;
//     if (minScore > subwordScore) {
//       minScore = subwordScore;
//     }
//   };

//   float sequence = 0.0;

//   for (const float& subwordScore : logProbs) {
//     sequence += subwordScore;

//     ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);
//     /// a word is composed by multiple subtokens. The EOS token
//     /// is the only one where subword.begin si the same as the end
//     /// Therefore, if both are equal, we have reached the end of sentence
//     /// If we do not apply an break statement the code would fail since
//     /// target.text do not contain the EOS token
//     if (subword.begin == sentenceByteRange.end) {  // EOS token
//       insertNewWord(subword, subwordScore, 1);
//       break;
//     }
//     char firstLetter = target.text.at(subword.begin);
//     if ((isspace(firstLetter) != 0) || subwordIdx == 0) {
//       if (subwordIdx != 0) {
//         subword.begin += 1;  // ignore whitespace
//       }
//       subword.end -= 1;  // remove whitepsace
//       bpeLen = 1;
//       ++wordIdx;
//       insertNewWord(subword, subwordScore, bpeLen);
//     } else {
//       bpeLen += 1;
//       argumentGivenWord(subword, subwordScore, bpeLen, wordIdx);
//     }
//     ++subwordIdx;
//   }

//   modelFeatures.overallMean = sequence / subwordIdx;

//   return {wordByteRanges, modelFeatures};
// }

std::vector<float> QualityEstimator::LogisticRegressor::predictWordScores(const IntgemmMatrix& features) const {
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

QualityEstimator::IntgemmMatrix QualityEstimator::LogisticRegressor::extractFeatures(
    const ModelFeatures& modelFeatures) const {
  const intgemm::Index numWords = modelFeatures.wordMeanScores.size();

  QualityEstimator::IntgemmMatrix features(numWords, NumberOfFeatures, intgemm::Int16::tile_info.a_rows,
                                           intgemm::Int16::tile_info.a_cols);

  const auto getStds = [](const auto stds) { return stds != 0.0 ? stds : 1.0f; };

  for (int i = 0; i < numWords; ++i) {
    features.data[features.cols * i + 0] =
        (modelFeatures.wordMeanScores[i] - scale_.means[0]) / getStds(scale_.stds[0]);
    features.data[features.cols * i + 1] = (modelFeatures.minWordScores[i] - scale_.means[1]) / getStds(scale_.stds[1]);
    features.data[features.cols * i + 2] = (modelFeatures.numSubWords[i] - scale_.means[2]) / getStds(scale_.stds[2]);
    features.data[features.cols * i + 3] = (modelFeatures.overallMean - scale_.means[3]) / getStds(scale_.stds[3]);
  }

  return features;
}

float QualityEstimator::LogisticRegressor::computeWordProbabilities(std::vector<float>& wordQualityScores) const {
  float sentenceScore = 0.0;

  for (float& wordScore : wordQualityScores) {
    wordScore = 1 / (1 + std::exp(-wordScore));
    sentenceScore += wordScore;
  }
  // TODO:
  // Should we include or remove the end of sentence (EOS) token?
  return sentenceScore / wordQualityScores.size();
}

QualityEstimator::WordsQualityEstimate QualityEstimator::LogisticRegressor::predictQualityScores(
    const std::vector<ByteRange>& wordByteRanges, const ModelFeatures& modelFeatures) const {
  std::vector<float> wordQualityScores = predictWordScores(extractFeatures(modelFeatures));
  const float sentenceScore = computeWordProbabilities(wordQualityScores);
  return {wordQualityScores, wordByteRanges, sentenceScore};
}

QualityEstimator::WordsQualityEstimate QualityEstimator::computeQualityScores(const std::vector<float>& logProbs,
                                                                              const AnnotatedText& target,
                                                                              const size_t sentenceIdx) const {
  auto [wordByteRanges, modelFeatures] = mapBPEToWords(logProbs, target, sentenceIdx);

  return logisticRegressor_.predictQualityScores(wordByteRanges, modelFeatures);
}
}  // namespace bergamot
}  // namespace marian
