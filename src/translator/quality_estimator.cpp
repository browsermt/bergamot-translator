#include "quality_estimator.h"

#include <iostream>
#include <numeric>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

constexpr intgemm::Index getNextMultiple(const intgemm::Index number, const intgemm::Index multiple) {
  const intgemm::Index modWidth = number % multiple;
  return (modWidth == 0) ? number : number + multiple - modWidth;
}

QualityEstimator::Matrix::Matrix(const size_t rowsParam, const size_t colsParam)
    : rows(rowsParam), cols(colsParam), data(rows * cols) {
  for (float& elem : data) {
    elem = 0.0;
  }
}

QualityEstimator::Matrix::Matrix(Matrix && other)
    : rows(other.rows), cols(other.cols), data(std::move(other.data)) {
}

const float& QualityEstimator::Matrix::at(const size_t row, const size_t col) const { return data[row * cols + col]; }

float& QualityEstimator::Matrix::at(const size_t row, const size_t col) { return data[row * cols + col]; }

QualityEstimator::IntgemmMatrix::IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index colsParam,
                                               const intgemm::Index rowsMultiplier, const intgemm::Index colsMultiplier)
    : Matrix(getNextMultiple(rowsParam, rowsMultiplier), getNextMultiple(colsParam, colsMultiplier)) {}

QualityEstimator::Matrix QualityEstimator::IntgemmMatrix::operator*(const IntgemmMatrix& matrixb) const {
  const float quant_mult = 1024.0f;

  AlignedVector<int16_t> A_prepared(data.size());
  AlignedVector<int16_t> B_prepared(matrixb.data.size());

  intgemm::Int16::PrepareA(data.begin(), A_prepared.begin(), quant_mult, rows, cols);
  intgemm::Int16::PrepareB(matrixb.data.begin(), B_prepared.begin(), quant_mult, matrixb.rows, matrixb.cols);

  Matrix result(rows, matrixb.cols);

  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), rows, cols, matrixb.cols,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), result.data.begin()));

  return result;
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

    ABORT_IF( scale.stds[i] == 0.0, "Invalid stds" );

    scale.means[i] = *(means + i);
    coefficientsMatrix.at(i, 0) = *(coefficients + i);
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
    const float coefficient = logisticRegressor_.coefficients.at(i, 0);
    memcpy(buffer, &coefficient, sizeof(coefficient));
    buffer += sizeof(coefficient);
  }

  memcpy(buffer, &logisticRegressor_.intercept, sizeof(logisticRegressor_.intercept));
  buffer += sizeof(logisticRegressor_.intercept);

  return memory;
}

// mapBPEToWords takes the following arguments:
// - the log probabilities (logProbs) of byte pair encodings (BPE)
//   that comes from the tracebackWordScores method (which belongs to hypothesis.h in Marian)
// - the AnnotatedText from the translated word
// - the index of a translated sentence
//
// This method is responsible for mapping BPE tokens to word tokens representing them through ByteRanges.
//
// The words byte ranges are expected to be alphanumeric characters, and they are split using whitespaces. Moreover,
// this method is also responsible for extracting the features that the QE model will further use. The features
// extracted are the following:
//
// - meanScore = the mean of bpe's  logProbs  that a given word corresponds to
// - minScore = the minimum bpe's logProbs that a given word corresponds to
// - numSubWords = the number of bpe tokens that a term is made of
// - overallMean = the mean of bpe's logProbs regarding the whole translated sentence
//
// The return of this function is a ByteRange vector of the words and a WordFeatures vector.
//
// If a translated sentence does not contain any alphanumeric character (therefore, it is made basically of the EOS
// token), this method ignores it and returns an empty ByteRange vector of words and an empty WordFeatures vector.
//
// Examples:
// Suppose that you have the following source target (A): marian is a good translation service and the translate service
// gives you the following sentence (B):
//
// ma(0.15) ri(0.15) an(0.2) es(0.3) un(0.1) bu(0.3) en(0.2) ser(0.1) vi(0.2) cio(0.4) de(0.1) tra(0.4) du(0.2)
// cción(0.1)
//
// The numbers that the words follow represent the logProb of each BPE token.
//
// Then, the result would be something like:
// a vector where each position corresponds to the ByteRanges of the following words: marian es un buen servicio de
// traducción. Hence, its length is 7.
//
// An vector of WordFeatures with length 7 where, for instance:
//
// the values of the first element (marian) would be:
// - meanScores= (0.15+0.15+0.3)/3=0.2
// - minScores= 0.15
// - numSubWords = 3
// - overallMean = 0.207
//
// the values of the second element (es) would be:
// - meanScores= 0.3
// - minScores= 0.3
// - numSubWords = 1
// - overallMean = 0.207
//
std::pair<std::vector<ByteRange>, QualityEstimator::Matrix > QualityEstimator::mapBPEToWords(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) {
  // Ignore empty target
  if ((logProbs.size() < 2 ) || ( target.numWords( sentenceIdx ) == 0 ) )
  {
    return {{}, Matrix( 0, 0 ) };
  }

  enum Features
  {
    MeanScore = 0,
    MinScore = 1,
    NumSubWords = 2,
    OverallMean  = 3
  };

  const string_view sentence = target.sentence( sentenceIdx );

  const size_t numWords = std::count( std::begin( sentence ), std::end( sentence ), ' ' ) + 1;

  const size_t numFeatures = 4;

  std::vector<ByteRange> wordByteRanges;

  Matrix features( numWords,numFeatures );

  // The first subword it's always a beginning of a word
  float subwordScore = logProbs[0];
  ByteRange subword = target.wordAsByteRange(sentenceIdx, 0);

  float sequence = subwordScore;

  size_t featureRow = 0;

  features.at( featureRow, MeanScore ) = subwordScore;
  features.at( featureRow, MinScore ) = subwordScore;
  features.at( featureRow, NumSubWords ) = 1;

  wordByteRanges.push_back(subword);

  size_t subwordIdx = 1;
  /// A word is composed of multiple subtokens. The definition of an "entire"
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
      ++featureRow;
      features.at( featureRow, MeanScore ) = subwordScore;
      features.at( featureRow, MinScore ) = subwordScore;
      features.at( featureRow, NumSubWords ) = 1;

      wordByteRanges.push_back(subword);
    } else {
      // update last word byte range and word features

      ByteRange& currentWord = wordByteRanges.back();

      float& meanScore = features.at( featureRow, MeanScore );
      float& minScore = features.at( featureRow, MinScore );
      float& numSubWords = features.at( featureRow, NumSubWords );

      currentWord.end = subword.end;
      ++numSubWords;
      // incremental mean
      meanScore += (subwordScore - meanScore) / numSubWords;

      if (minScore > subwordScore) {
        minScore = subwordScore;
      }
    }
  }

  const float overallMean = sequence / subwordIdx;

  for( int i = 0; i < features.rows ; ++i )
  {
    features.at( i, OverallMean ) = overallMean;
  }

  return { wordByteRanges, std::move(features) };
}

std::vector<float> QualityEstimator::LogisticRegressor::vectorResult(const IntgemmMatrix& features) const {
  const Matrix modelScores = features * coefficients;

  std::vector<float> scores(features.rows);

  for (int i = 0; i < modelScores.rows; ++i) {
    scores[i] = modelScores.at(i, 0) + intercept;
  }

  return scores;
}

QualityEstimator::IntgemmMatrix QualityEstimator::LogisticRegressor::transformFeatures(const Matrix& features) const {
  QualityEstimator::IntgemmMatrix resultFeatures(features.rows, features.cols, intgemm::Int16::tile_info.a_rows,
                                                 intgemm::Int16::tile_info.a_cols);
  for (int i = 0; i < features.rows; ++i) {
    for (int j = 0; j < features.cols; ++j) {
      resultFeatures.at(i, j) = (features.at(i, j) - scale.means[j]) / scale.stds[j];
    }
  }

  return resultFeatures;
}

void QualityEstimator::LogisticRegressor::resultToProbabilities(std::vector<float>& linearPredictedValues) const {
  for (float& value : linearPredictedValues) {
    value = 1 / (1 + std::exp(-value));
  }
}

std::vector<float> QualityEstimator::LogisticRegressor::predict(const Matrix& features) const {
  std::vector<float> scores = vectorResult(transformFeatures(features));
  resultToProbabilities(scores);
  return scores;
}

QualityEstimator::WordsQualityEstimate QualityEstimator::computeQualityScores(const std::vector<float>& logProbs,
                                                                              const AnnotatedText& target,
                                                                              const size_t sentenceIdx) const {
  const auto [wordByteRanges, features] = mapBPEToWords(logProbs, target, sentenceIdx);

  const auto wordQualityScores = logisticRegressor_.predict(features);

  const auto sentenceScore = std::accumulate(std::begin(wordQualityScores), std::end(wordQualityScores), float(0.0)) /
                             wordQualityScores.size();

  return {wordQualityScores, wordByteRanges, sentenceScore};
}
}  // namespace bergamot
}  // namespace marian
