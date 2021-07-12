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
  const int numParameters = 4;
  stds_ = begin;
  means_ = (begin += numFeatures_);
  coefficients_ = (begin += numFeatures_);
  intercept_ = (begin += numFeatures_);
  modelParameters_.resize(numParameters * numFeatures_);
  //[0,1,2,3]-> stds, means, coefficients, intercept
  for (int i = 0; i < numFeatures_; i++) {
    modelParameters_[i + 0 * numFeatures_] = stds_[i];
    modelParameters_[i + 1 * numFeatures_] = means_[i];
    modelParameters_[i + 2 * numFeatures_] = coefficients_[i];
    modelParameters_[i + 3 * numFeatures_] = intercept_[i];
  }
}

QualityEstimator::SentenceQualityEstimate QualityEstimator::mapBPEToWords(Quality quality, AnnotatedText target,
                                                                          size_t sentenceIdx) const {
  const ByteRange& sentenceByteRange = target.sentenceAsByteRange(sentenceIdx);
  QualityEstimator::SentenceQualityEstimate sentenceQualityScores;
  int bpeLen = 1;
  int wordIdx = 0;
  int subwordIdx = wordIdx;
  float tmp;
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
  overallMean_ = quality.sequence / subwordIdx;
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

AlignedVector<float> QualityEstimator::predictWordScores(AlignedVector<float>& featureMatrix, int numWords) const {
  const intgemm::Index featureRows = numWords;
  const intgemm::Index width = 64;
  const intgemm::Index modelRows = 8;
  const AlignedVector<float>& modelMatrix = QualityEstimator::buildLogisticModel();
  float quant_mult = 1024.0f;
  AlignedVector<int16_t> A_prepared(featureMatrix.size());
  AlignedVector<int16_t> B_prepared(modelMatrix.size());
  // Quantize A.
  // TODO
  // I dont know why is this failing
  intgemm::Int16::PrepareA(featureMatrix.begin(), A_prepared.begin(), quant_mult, numWords, width);
  // Quantize B.
  // TODO
  // I dont know why is this failing
  intgemm::Int16::PrepareB(modelMatrix.begin(), B_prepared.begin(), quant_mult, width, modelRows);

  AlignedVector<float> modelScores(numWords);
  intgemm::Int16::Multiply(
      A_prepared.begin(), B_prepared.begin(), numWords, width, modelRows,
      intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), modelScores.begin()));

  return modelScores;
}

AlignedVector<float> QualityEstimator::buildLogisticModel() const {
  const intgemm::Index width = 64;
  const intgemm::Index modelColumn = 8;
  AlignedVector<float> modelMatrix(width * modelColumn);
  const int& coefficientsStart = 2 * numFeatures_;
  for (int i = 0; i < numFeatures_; i++) {
    modelMatrix[i] = modelParameters_[coefficientsStart + i];
  }
  return modelMatrix;
}

AlignedVector<float> QualityEstimator::extractFeatures(QualityEstimator::SentenceQualityEstimate qualityScores) const {
  const intgemm::Index numWords = qualityScores.wordQualityScores.size();
  const intgemm::Index width = 64;
  AlignedVector<float> featureMatrix(1 * width);
  const int& stdStart = 0;
  const int& meanStart = 1 * numFeatures_;
  for (int i = 0; i < numWords; i++) {
    // scale features
    featureMatrix[i] = (qualityScores.wordQualityScores[i] - modelParameters_[meanStart]) / modelParameters_[stdStart];
    featureMatrix[i + 1 * numFeatures_] =
        (minWordScore_[i] - modelParameters_[meanStart + 1]) / modelParameters_[stdStart + 1];
    featureMatrix[i + 2 * numFeatures_] =
        (numSubWords_[i] - modelParameters_[meanStart + 2]) / modelParameters_[stdStart + 2];
    featureMatrix[i + 3 * numFeatures_] =
        (overallMean_ - modelParameters_[meanStart + 3]) / modelParameters_[stdStart + 3];
  }

  return featureMatrix;
}

void QualityEstimator::computeQualityScores(Quality& quality, AnnotatedText& target, size_t sentenceIdx) const {
  auto qualityScores = mapBPEToWords(quality, target, sentenceIdx);
  auto featureMatrix = extractFeatures(std::move(qualityScores));
  predictWordScores(featureMatrix, qualityScores.wordQualityScores.size());
}

}  // namespace bergamot
}  // namespace marian
