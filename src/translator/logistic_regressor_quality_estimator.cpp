#include "logistic_regressor_quality_estimator.h"

#include <algorithm>
#include <numeric>

namespace marian::bergamot {

// Given an input matrix $\mathbf{X}$, the usual Logistic Regression calculus can be seen as the following:
//
// 1) Standardize it, returning in $\mathbf{Z} = \frac{(\mathbf{X}-\mu)}{\sigma}$, where $\mu$ stands for the mean
// vector and $\sigma$ represents the standard deviation
//
// 2) Then, we apply $\sum_{i=1}^{D}{ w_i z_i}$, where $D$ is the dimension (i.e. the number of features) and $w$ is the
// model vector with learnt weights
//
// 3) We apply the sigmoid function to the result
//
// Notice, however, that for the first two steps we can do the following:
//
// \begin{align*}
// \sum_{i=1}^{D}{ w_i z_i} &= \mathbf{w^T}\left(\mathbf{\sigma^{-1}} \odot (\mathbf{x} - \mathbf{\mu})\right) \text{ //
// we are just vectoring step 1}\\
//      &= \sum_{i=1}^{D}{\sigma_i^{-1} w_i (x_i - \mu_i)} \\
//      &= \sum_{i=1}^{D}{\sigma_i^{-1} w_ix_i - \sigma_i^{-1} w_i \mu_i} \\
//      &= \sum_{i=1}^{D}{\left(\sigma_i^{-1} w_i\right)x_i - \left(\sigma_i^{-1} w_i \mu_i\right)}
// \end{align*}
// Then, $(\sigma_i^{-1} w_i \mu_i)$ can be precomputed without any dependence on inference data. This is done by the
// variable $\textit{constantFactor_}$ and $\textit{intercept_}$ in the code.
//
// For a better reading please refer to: http://mathb.in/63082

LogisticRegressorQualityEstimator::LogisticRegressorQualityEstimator(Scale&& scale, std::vector<float>&& coefficients,
                                                                     const float intercept)
    : IQualityEstimator(),
      scale_(std::move(scale)),
      coefficients_(std::move(coefficients)),
      intercept_(intercept),
      coefficientsByStds_(coefficients_.size()) {
  ABORT_IF(scale_.means.size() != scale_.stds.size(), "Number of means is not equal to number of stds");
  ABORT_IF(scale_.means.size() != coefficients_.size(), "Number of means is not equal to number of coefficients");

  // Pre-compute the scale operations for the linear model
  for (int i = 0; i < coefficients_.size(); ++i) {
    coefficientsByStds_[i] = coefficients_[i] / scale_.stds[i];
    constantFactor_ += coefficientsByStds_[i] * scale_.means[i];
  }
}

LogisticRegressorQualityEstimator::LogisticRegressorQualityEstimator(LogisticRegressorQualityEstimator&& other)
    : IQualityEstimator(),
      scale_(std::move(other.scale_)),
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
  const float* coefficientsMemory = memoryIndex += header.lrParametersDims;
  const float intercept = *(memoryIndex += header.lrParametersDims);

  Scale scale;
  scale.means.resize(header.lrParametersDims);
  scale.stds.resize(header.lrParametersDims);

  std::vector<float> coefficients(header.lrParametersDims);

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

void LogisticRegressorQualityEstimator::computeQualityScores(Response& response, const Histories& histories) const {
  size_t sentenceIndex = 0;

  for (const auto& history : histories) {
    const auto logProbs = std::get<1>(history->top())->tracebackWordScores();
    response.qualityScores.push_back(computeSentenceScores(logProbs, response.target, sentenceIndex));

    ++sentenceIndex;
  }
}

Response::WordsQualityEstimate LogisticRegressorQualityEstimator::computeSentenceScores(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) const

{
  const auto [wordBytesRanges, wordslogProbs] = remapWordsAndLogProbs(logProbs, target, sentenceIdx);

  const auto wordQualityScores = predict(extractFeatures(wordslogProbs));

  const float sentenceScore = std::accumulate(std::begin(wordQualityScores), std::end(wordQualityScores), float(0.0)) /
                              wordQualityScores.size();

  return {wordQualityScores, wordBytesRanges, sentenceScore};
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

Matrix LogisticRegressorQualityEstimator::extractFeatures(const std::vector<std::vector<float> >& wordsLogProbs) {
  if (wordsLogProbs.empty()) {
    return std::move(Matrix(0, 0));
  }

  Matrix features(wordsLogProbs.size(), /*numFeatures =*/4);
  size_t featureRow = 0;
  const size_t I_MEAN{0}, I_MIN{1}, I_NUM_SUBWORDS{2}, I_OVERALL_MEAN{3};

  float overallMean = 0.0;
  size_t numlogProbs = 0;

  for (const auto& wordLogProbs : wordsLogProbs) {
    if (wordLogProbs.size() == 0) {
      ++featureRow;
      continue;
    }

    float minScore = std::numeric_limits<float>::max();

    for (const auto& logProb : wordLogProbs) {
      ++numlogProbs;
      overallMean += logProb;
      features.at(featureRow, I_MEAN) += logProb;

      if (logProb < minScore) {
        minScore = logProb;
      }
    }

    features.at(featureRow, I_MEAN) /= static_cast<float>(wordLogProbs.size());
    features.at(featureRow, I_MIN) = minScore;
    features.at(featureRow, I_NUM_SUBWORDS) = wordLogProbs.size();

    ++featureRow;
  }

  if (numlogProbs == 0) {
    return std::move(Matrix(0, 0));
  }

  overallMean /= numlogProbs;

  for (int i = 0; i < features.rows; ++i) {
    features.at(i, I_OVERALL_MEAN) = overallMean;
  }

  return std::move(features);
}

}  // namespace marian::bergamot
