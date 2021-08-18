#pragma once

#include <vector>

#include "definitions.h"
#include "iquality_model.h"
#include "matrix.h"

namespace marian::bergamot {
// ASCII and Unicode text files never start with the following 64 bits
constexpr std::size_t BINARY_QE_MODEL_MAGIC = 0x78cc336f1d54b180;
/// The current Quality Estimator model is a Logistic Model implemented through
/// a linear regressor + sigmoid function. Simply speaking, an LR model depends
/// on features to be scaled, so it contains four elements of data: a vector of coefficients
/// and an intercept (which represents the linear model) and a vector of means and stds
/// (which are necessary for feature scaling).
///
/// These variables are firstly initialized by parsing a file (which comes from memory),
/// and then they are used to build a model representation
///
class LogisticRegressor : public IQualityModel {
 public:
  struct Header {
    uint64_t magic;             // BINARY_QE_MODEL_MAGIC
    uint64_t lrParametersDims;  // Length of lr parameters stds, means and coefficients .
  };

  struct Scale {
    std::vector<float> stds;
    std::vector<float> means;
  };

  LogisticRegressor(Scale &&scale, std::vector<float> &&coefficients, const float intercept);

  LogisticRegressor(LogisticRegressor &&other);

  /// Binary file parser which came from AlignedMemory
  /// It's expected from AlignedMemory the following structure:
  /// - a header with the number of parameters dimensions
  /// - a vector of standard deviations of features
  /// - a vector of means of features
  /// - a vector of coefficients
  /// - a intercept value
  static LogisticRegressor fromAlignedMemory(const AlignedMemory &alignedMemory);
  AlignedMemory toAlignedMemory() const;

  std::vector<float> predict(const Matrix &features) const override;

 private:
  Scale scale_;
  std::vector<float> coefficients_;
  float intercept_;
  float constantFactor_ = 0.0;
};

}  // namespace marian::bergamot