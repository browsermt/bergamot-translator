#pragma once

#include <vector>

#include "definitions.h"
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
class LogisticRegressor {
 public:
  struct Header {
    uint64_t magic;             // BINARY_QE_MODEL_MAGIC
    uint64_t lrParametersDims;  // Length of lr parameters stds, means and coefficients .
  };

  struct Scale {
    std::vector<float> stds;
    std::vector<float> means;
  };

  LogisticRegressor(Scale &&scale, IntgemmMatrix &&coefficients, const size_t numCoefficients, const float intercept);

  LogisticRegressor(LogisticRegressor &&other);

  /// binary file parser which came from AlignedMemory
  static LogisticRegressor fromAlignedMemory(const AlignedMemory &qualityEstimatorMemory);
  AlignedMemory toAlignedMemory() const;

  std::vector<float> predict(const Matrix &features) const;

 private:
  Scale scale_;
  IntgemmMatrix coefficients_;
  size_t numCoefficients_;
  float intercept_;

  /// Calculates the scores through a linear model
  /// @param [in] features: the matrix of feature scaled values
  std::vector<float> vectorResult(const IntgemmMatrix &features) const;

  /// It scale the values from feature matrix such that all columns have std 1 and mean 0
  /// @param [in] features: a struct matrix with the features values
  IntgemmMatrix transformFeatures(const Matrix &features) const;

  /// Applies a sigmoid function to each element of a vector and returns the mean of the result vector
  /// @param [in] linearPredictedValues: the vector of real values returned by a linear regression
  void resultToProbabilities(std::vector<float> &linearPredictedValues) const;
};

}  // namespace marian::bergamot