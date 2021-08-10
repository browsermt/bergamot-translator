#include "logistic_regressor.h"

namespace marian::bergamot {

LogisticRegressor::LogisticRegressor(Scale&& scale, IntgemmMatrix&& coefficients, const size_t numCoefficients,
                                     const float intercept)
    : scale_(std::move(scale)),
      coefficients_(std::move(coefficients)),
      numCoefficients_(numCoefficients),
      intercept_(intercept) {
  ABORT_IF(scale_.means.size() != scale_.stds.size(), "Number of means is not equal to number of stds");
  ABORT_IF(scale_.means.size() != numCoefficients, "Number of means is not equal to number of coefficients");
}

LogisticRegressor::LogisticRegressor(LogisticRegressor&& other)
    : scale_(std::move(other.scale_)),
      coefficients_(std::move(other.coefficients_)),
      numCoefficients_(std::move(other.numCoefficients_)),
      intercept_(std::move(other.intercept_)) {}

LogisticRegressor LogisticRegressor::fromAlignedMemory(const AlignedMemory& qualityEstimatorMemory) {
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

    ABORT_IF(scale.stds[i] == 0.0, "Invalid stds");

    scale.means[i] = *(means + i);
    coefficientsMatrix.at(i, 0) = *(coefficients + i);
  }

  return LogisticRegressor(std::move(scale), std::move(coefficientsMatrix), header.lrParametersDims, intercept);
}

AlignedMemory LogisticRegressor::toAlignedMemory() const {
  const size_t lrParametersDims = scale_.means.size();

  const size_t lrSize =
      (scale_.means.size() + scale_.stds.size() + numCoefficients_) * sizeof(float) + sizeof(intercept_);

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
    const float coefficient = coefficients_.at(i, 0);
    memcpy(buffer, &coefficient, sizeof(coefficient));
    buffer += sizeof(coefficient);
  }

  memcpy(buffer, &intercept_, sizeof(intercept_));
  buffer += sizeof(intercept_);

  return memory;
}

std::vector<float> LogisticRegressor::predict(const Matrix& features) const {
  std::vector<float> scores = vectorResult(transformFeatures(features));
  resultToProbabilities(scores);
  return scores;
}

IntgemmMatrix LogisticRegressor::transformFeatures(const Matrix& features) const {
  IntgemmMatrix resultFeatures(features.rows, features.cols, intgemm::Int16::tile_info.a_rows,
                               intgemm::Int16::tile_info.a_cols);
  for (int i = 0; i < features.rows; ++i) {
    for (int j = 0; j < features.cols; ++j) {
      resultFeatures.at(i, j) = (features.at(i, j) - scale_.means[j]) / scale_.stds[j];
    }
  }

  return resultFeatures;
}

std::vector<float> LogisticRegressor::vectorResult(const IntgemmMatrix& features) const {
  const Matrix modelScores = features * coefficients_;

  std::vector<float> scores(features.rows);

  for (int i = 0; i < modelScores.rows; ++i) {
    scores[i] = modelScores.at(i, 0) + intercept_;
  }

  return scores;
}

void LogisticRegressor::resultToProbabilities(std::vector<float>& linearPredictedValues) const {
  for (float& value : linearPredictedValues) {
    value = 1 / (1 + std::exp(-value));
  }
}

}  // namespace marian::bergamot