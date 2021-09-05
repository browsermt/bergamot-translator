#include "quality_estimator.h"

namespace marian::bergamot {

void UnsupervisedQualityEstimator::computeQualityScores(const Histories& histories, Response& response) const {
  for (size_t i = 0; i < histories.size(); ++i) {
    const Result result = histories[i]->top();
    const Hypothesis::PtrType& hypothesis = std::get<1>(result);
    const std::vector<float> logProbs = hypothesis->tracebackWordScores();
    response.qualityScores.push_back(std::move(computeSentenceScores(logProbs, response.target, i)));
  }
}

Response::SentenceQualityScore UnsupervisedQualityEstimator::computeSentenceScores(const std::vector<float>& logProbs,
                                                                                   const AnnotatedText& target,
                                                                                   const size_t sentenceIdx) const {
  const std::vector<SubwordRange> wordIndexes = mapWords(logProbs, target, sentenceIdx);

  std::vector<float> wordScores;

  for (const SubwordRange& wordIndex : wordIndexes) {
    wordScores.push_back(
        std::accumulate(logProbs.begin() + wordIndex.begin, logProbs.begin() + wordIndex.end, float(0.0)) /
        wordIndex.size());
  }

  const float sentenceScore =
      std::accumulate(std::begin(wordScores), std::end(wordScores), float(0.0)) / wordScores.size();

  return {wordScores, subwordToWords(wordIndexes, target, sentenceIdx), sentenceScore};
}

LogisticRegressorQualityEstimator::Matrix::Matrix(const size_t rowsParam, const size_t colsParam)
    : rows(rowsParam), cols(colsParam), data_(rowsParam * colsParam) {}

LogisticRegressorQualityEstimator::Matrix::Matrix(Matrix&& other)
    : rows(other.rows), cols(other.cols), data_(std::move(other.data_)) {}

const float& LogisticRegressorQualityEstimator::Matrix::at(const size_t row, const size_t col) const {
  return data_[row * cols + col];
}

float& LogisticRegressorQualityEstimator::Matrix::at(const size_t row, const size_t col) {
  return data_[row * cols + col];
}

LogisticRegressorQualityEstimator::LogisticRegressorQualityEstimator(Scale&& scale, Array&& coefficients,
                                                                     const float intercept)
    : scale_(std::move(scale)), coefficients_(std::move(coefficients)), intercept_(intercept), coefficientsByStds_() {
  // Pre-compute the scale operations for the linear model
  for (int i = 0; i < coefficients_.size(); ++i) {
    coefficientsByStds_[i] = coefficients_[i] / scale_.stds[i];
    constantFactor_ += coefficientsByStds_[i] * scale_.means[i];
  }
}

LogisticRegressorQualityEstimator::LogisticRegressorQualityEstimator(LogisticRegressorQualityEstimator&& other)
    : scale_(std::move(other.scale_)),
      coefficients_(std::move(other.coefficients_)),
      intercept_(std::move(other.intercept_)),
      coefficientsByStds_(std::move(other.coefficientsByStds_)),
      constantFactor_(std::move(other.constantFactor_)) {}

LogisticRegressorQualityEstimator LogisticRegressorQualityEstimator::fromAlignedMemory(
    const AlignedMemory& alignedMemory) {
  LOG(info, "[data] Loading Quality Estimator model from buffer");

  const char* ptr = alignedMemory.begin();
  const size_t blobSize = alignedMemory.size();

  ABORT_IF(blobSize < sizeof(Header), "Quality estimation file too small");
  const Header& header = *reinterpret_cast<const Header*>(ptr);

  ABORT_IF(header.magic != BINARY_QE_MODEL_MAGIC, "Incorrect magic bytes for quality estimation file");
  ABORT_IF(header.lrParametersDims <= 0, "The number of lr parameter dimension cannot be equal or less than zero");

  const uint64_t expectedSize =
      sizeof(Header) + (numLrParamsWithDimension_ * header.lrParametersDims + numIntercept_) * sizeof(float);
  ABORT_IF(expectedSize != blobSize, "QE header claims file size should be {} bytes but file is {} bytes", expectedSize,
           blobSize);

  ptr += sizeof(Header);
  const float* memoryIndex = reinterpret_cast<const float*>(ptr);

  const float* stds = memoryIndex;
  const float* means = memoryIndex += header.lrParametersDims;
  const float* coefficientsMemory = memoryIndex += header.lrParametersDims;
  const float intercept = *(memoryIndex += header.lrParametersDims);

  Scale scale;

  Array coefficients;

  for (int i = 0; i < header.lrParametersDims; ++i) {
    scale.stds[i] = *(stds + i);

    ABORT_IF(scale.stds[i] == 0.0, "Invalid stds");

    scale.means[i] = *(means + i);
    coefficients[i] = *(coefficientsMemory + i);
  }

  return LogisticRegressorQualityEstimator(std::move(scale), std::move(coefficients), intercept);
}

AlignedMemory LogisticRegressorQualityEstimator::toAlignedMemory() const {
  const size_t lrParametersDims = scale_.means.size();

  const size_t lrSize =
      (scale_.means.size() + scale_.stds.size() + coefficients_.size()) * sizeof(float) + sizeof(intercept_);

  Header header = {BINARY_QE_MODEL_MAGIC, lrParametersDims};
  marian::bergamot::AlignedMemory memory(sizeof(header) + lrSize);

  char* buffer = memory.begin();

  memcpy(buffer, &header, sizeof(header));
  buffer += sizeof(header);

  for (const float std : scale_.stds) {
    memcpy(buffer, &std, sizeof(std));
    buffer += sizeof(std);
  }

  for (const float mean : scale_.means) {
    memcpy(buffer, &mean, sizeof(mean));
    buffer += sizeof(mean);
  }

  for (size_t i = 0; i < lrParametersDims; ++i) {
    const float coefficient = coefficients_[i];
    memcpy(buffer, &coefficient, sizeof(coefficient));
    buffer += sizeof(coefficient);
  }

  memcpy(buffer, &intercept_, sizeof(intercept_));
  buffer += sizeof(intercept_);

  return memory;
}

void LogisticRegressorQualityEstimator::computeQualityScores(const Histories& histories, Response& response) const {
  for (size_t i = 0; i < histories.size(); ++i) {
    const Result result = histories[i]->top();
    const Hypothesis::PtrType& hypothesis = std::get<1>(result);
    const std::vector<float> logProbs = hypothesis->tracebackWordScores();

    response.qualityScores.push_back(std::move(computeSentenceScores(logProbs, response.target, i)));
  }
}

Response::SentenceQualityScore LogisticRegressorQualityEstimator::computeSentenceScores(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) const

{
  const std::vector<SubwordRange> wordIndexes = mapWords(logProbs, target, sentenceIdx);

  const std::vector<float> wordScores = predict(extractFeatures(wordIndexes, logProbs));

  const float sentenceScore =
      std::accumulate(std::begin(wordScores), std::end(wordScores), float(0.0)) / wordScores.size();

  return {wordScores, subwordToWords(wordIndexes, target, sentenceIdx), sentenceScore};
}

std::vector<float> LogisticRegressorQualityEstimator::predict(const Matrix& features) const {
  std::vector<float> scores(features.rows);

  for (int i = 0; i < features.rows; ++i) {
    for (int j = 0; j < features.cols; ++j) {
      scores[i] += features.at(i, j) * coefficientsByStds_[j];
    }
  }

  /// Applies the linear model followed by a sigmoid function to each element

  for (int i = 0; i < features.rows; ++i) {
    scores[i] = std::log(1 - (1 / (1 + std::exp(-(scores[i] - constantFactor_ + intercept_)))));
  }

  return scores;
}

LogisticRegressorQualityEstimator::Matrix LogisticRegressorQualityEstimator::extractFeatures(
    const std::vector<SubwordRange>& wordIndexes, const std::vector<float>& logProbs) const {
  if (wordIndexes.empty()) {
    return std::move(Matrix(0, 0));
  }

  Matrix features(wordIndexes.size(), /*numFeatures =*/4);
  size_t featureRow = 0;
  const size_t I_MEAN{0}, I_MIN{1}, I_NUM_SUBWORDS{2}, I_OVERALL_MEAN{3};

  float overallMean = 0.0;
  size_t numlogProbs = 0;

  for (const SubwordRange& wordIndex : wordIndexes) {
    if (wordIndex.begin == wordIndex.end) {
      ++featureRow;
      continue;
    }

    float minScore = std::numeric_limits<float>::max();

    for (size_t i = wordIndex.begin; i < wordIndex.end; ++i) {
      ++numlogProbs;
      overallMean += logProbs[i];
      features.at(featureRow, I_MEAN) += logProbs[i];

      minScore = std::min<float>(logProbs[i], minScore);
    }

    features.at(featureRow, I_MEAN) /= static_cast<float>(wordIndex.size());
    features.at(featureRow, I_MIN) = minScore;
    features.at(featureRow, I_NUM_SUBWORDS) = wordIndex.size();

    ++featureRow;
  }

  if (numlogProbs == 0) {
    return std::move(Matrix(0, 0));
  }

  overallMean /= wordIndexes.rbegin()->end;

  for (int i = 0; i < features.rows; ++i) {
    features.at(i, I_OVERALL_MEAN) = overallMean;
  }

  return std::move(features);
}

std::vector<SubwordRange> mapWords(const std::vector<float>& logProbs, const AnnotatedText& target,
                                   const size_t sentenceIdx) {
  // Ignore empty target
  if ((logProbs.size() < 2) || (target.numWords(sentenceIdx) == 0)) {
    return {};
  }
  // It is expected that translated words will have at least one word
  std::vector<SubwordRange> wordIndexes(/*numWords=*/1);

  /// The LogisticRegressorQualityEstimator model ignores the presence of the EOS token, and hence we only need to
  /// iterate n-1 positions.
  for (size_t subwordIdx = 0; subwordIdx < (logProbs.size() - 1); ++subwordIdx) {
    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    const char firstLetter = target.text.at(subword.begin);

    // if the first character is whitespace, it's a beginning of a new word
    if (isspace(firstLetter)) {
      wordIndexes.back().end = subwordIdx;
      wordIndexes.emplace_back();
      wordIndexes.back().begin = subwordIdx;
    }
  }

  wordIndexes.back().end = logProbs.size() - 1;

  return wordIndexes;
}

std::vector<ByteRange> subwordToWords(const std::vector<SubwordRange>& wordIndexes, const AnnotatedText& target,
                                      const size_t sentenceIdx) {
  std::vector<ByteRange> words;

  for (const SubwordRange& wordIndex : wordIndexes) {
    size_t wordBegin = target.wordAsByteRange(sentenceIdx, wordIndex.begin).begin;
    size_t wordEnd = target.wordAsByteRange(sentenceIdx, wordIndex.end).begin;

    if (isspace(target.text.at(wordBegin))) {
      ++wordBegin;
    }

    words.emplace_back(ByteRange{wordBegin, wordEnd});
  }

  return words;
}

}  // namespace marian::bergamot
