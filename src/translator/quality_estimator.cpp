#include "quality_estimator.h"

#include <iostream>

#include "byte_array_util.h"

namespace marian {
namespace bergamot {

QualityEstimator::QualityEstimator(AlignedMemory&& qualityEstimatorMemory)
  : memory_( std::move(qualityEstimatorMemory) )
{
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

QualityEstimator::SentenceQualityEstimate QualityEstimator::mapBPEToWords( const Quality& quality, const AnnotatedText& target,
                                                                           const size_t sentenceIdx) const {
  const auto sentenceByteRange = target.sentenceAsByteRange(sentenceIdx);
  SentenceQualityEstimate sentenceQualityScores;
  int bpeLen = 1;
  int wordIdx = 0;
  int subwordIdx = wordIdx;
  float tmp;

  for ( const float& subwordScore : quality.word) {
    auto subword = target.wordAsByteRange(sentenceIdx, subwordIdx);
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

  // sentenceQualityScores.wordByteRanges.pop_back();
  // sentenceQualityScores.wordQualityScores.pop_back();

  return sentenceQualityScores;
}

void QualityEstimator::insertNewWord( SentenceQualityEstimate& sentenceQualityScores,
                                     const ByteRange& subword, const float subwordScore, const int lenSubwords) const {
  numSubWords_.push_back(lenSubwords);
  minWordScore_.push_back(subwordScore);
  sentenceQualityScores.wordByteRanges.push_back(subword);
  sentenceQualityScores.wordQualityScores.push_back(subwordScore);
}

void QualityEstimator::augumentGivenWord(SentenceQualityEstimate& sentenceQualityScores, const ByteRange& subword,
                                         const float subwordScore, const int lenSubwords, const int wordIndex) const {
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

AlignedVector<float> QualityEstimator::predictWordScores( AlignedVector<float>& featureMatrix, const int numWords) const {
  const intgemm::Index featureRows = numWords;
  // const intgemm::Index width = 64;
  // const intgemm::Index modelRows = numFeatures_;
  auto modelMatrix = QualityEstimator::buildLogisticModel();
  float quant_mult = 1024.0f;

  float top_left_reference = 0.0f;
  for (int w = 0; w < 32; ++w) {
    top_left_reference += featureMatrix[w] * modelMatrix[w * 8];
  }

  // AlignedVector<int16_t> A_prepared(featureMatrix.size() );
  // AlignedVector<int16_t> B_prepared(modelMatrix.size()  );

  // intgemm::Int16::PrepareA(featureMatrix.begin(), A_prepared.begin(), quant_mult, numWords, 32);
  // intgemm::Int16::PrepareB(modelMatrix.begin(), B_prepared.begin(), quant_mult, 32, 8 );

   AlignedVector<float> modelScores(numWords*8);
  // intgemm::Int16::Multiply(
  //     A_prepared.begin(), B_prepared.begin(), numWords, 32, 8,
  //     intgemm::callbacks::UnquantizeAndWrite(1.0f / (quant_mult * quant_mult), modelScores.begin()));

  return modelScores;
}

AlignedVector<float> QualityEstimator::buildLogisticModel() const {
  // const intgemm::Index width = 64;
  // const intgemm::Index modelColumn = 8;

  // AlignedVector<float> modelMatrix( numFeatures_ * 1 );

  AlignedVector<float> modelMatrix( 32 * 8 );

  for (int i = 0; i < numFeatures_; i++) {
    modelMatrix[i* 8] = *(coefficients_+ i);
  }

  return modelMatrix;
}

AlignedVector<float> QualityEstimator::extractFeatures( const SentenceQualityEstimate& qualityScores) const {
  const intgemm::Index numWords = qualityScores.wordQualityScores.size();
  // const intgemm::Index width = 64;
  // AlignedVector<float> featureMatrix( numWords  * numFeatures_ );

  AlignedVector<float> featureMatrix( 192 );

  const int stdStart = 0;
  const int meanStart = 1 * numFeatures_;
  for (int i = 0; i < numWords; i++) {
    featureMatrix[i* numFeatures_ + 0 ] = (qualityScores.wordQualityScores[i] - *means_ ) / *(stds_ + 0);
    featureMatrix[i* numFeatures_ + 1 ] = (minWordScore_[i] - *(means_ + 1)  ) / *(stds_ + 1);
    featureMatrix[i* numFeatures_ + 2 ] = (numSubWords_[i] - *(means_+ 2) ) / *(stds_ + 2);
    featureMatrix[i* numFeatures_ + 3 ] = (overallMean_ - *(means_+ 3) )  / *(stds_ + 3);
  }

  return featureMatrix;
}

void QualityEstimator::computeQualityScores( const Quality& quality, const AnnotatedText& target, const size_t sentenceIdx) const {

  const std::vector< float > numbers = { -0.3, -0.0001, -0.002, -0.5, -0.2, -0.1, -0.001 };

  float sequence =  0.0;

  for( const auto number : numbers )
  {
    sequence += number;
  }

  const Quality qualityTemp = {  sequence, numbers };
  // const AnnotatedText targetTemp( "Es un ejemplo." );

  const auto qualityScores = mapBPEToWords(qualityTemp, target, sentenceIdx);
  auto featureMatrix = extractFeatures(qualityScores);
  predictWordScores(featureMatrix, qualityScores.wordQualityScores.size());
}

}  // namespace bergamot
}  // namespace marian
