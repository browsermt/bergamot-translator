#pragma once

#include <vector>
#include <boost/numeric/ublas/matrix.hpp>

#include "definitions.h"
#include "iquality_estimator.h"
// #include "matrix.h"

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
class LogisticRegressorQE : public IQualityEstimator {
  friend class LogisticRegressorQETest;

 public:

  using Matrix = boost::numeric::ublas::matrix< float >;
  struct Header {
    uint64_t magic;             // BINARY_QE_MODEL_MAGIC
    uint64_t lrParametersDims;  // Length of lr parameters stds, means and coefficients .
  };

  struct Scale {
    std::vector<float> stds;
    std::vector<float> means;
  };

  LogisticRegressorQE(Scale &&scale, std::vector<float> &&coefficients, const float intercept);

  LogisticRegressorQE(LogisticRegressorQE &&other);

  /// Binary file parser which came from AlignedMemory
  /// It's expected from AlignedMemory the following structure:
  /// - a header with the number of parameters dimensions
  /// - a vector of standard deviations of features
  /// - a vector of means of features
  /// - a vector of coefficients
  /// - a intercept value
  static LogisticRegressorQE fromAlignedMemory(const AlignedMemory &alignedMemory);
  AlignedMemory toAlignedMemory() const;

  void computeQualityScores(Response &response, const Histories &histories) const override;

 private:
  Scale scale_;
  std::vector<float> coefficients_;
  float intercept_;
  std::vector<float> coefficientsByStds_;
  float constantFactor_ = 0.0;

  std::vector<float> predict(const Matrix &features) const;

  /// construct the struct WordsQualityEstimate
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  Response::WordsQualityEstimate computeSentenceScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                       const size_t sentenceIdx) const;

  static boost::numeric::ublas::matrix< float > extractFeatures(const std::vector<std::vector<float> > &wordLogProbs);
};

}  // namespace marian::bergamot
