#pragma once

#include <array>
#include <vector>

#include "annotation.h"
#include "response.h"
#include "translator/history.h"

namespace marian::bergamot {

class QualityEstimator {
 public:
  /// Computes quality-scores using values from Histories and subword tokens which comes from Response
  ///
  ///
  /// @param [in] histories: Histories obtained from translating a blob of source-text
  /// @param [in] response: Partially constructed response, holding tokenization info
  /// for source and target. The quality-scores for each sentence obtained from source-text blob
  /// are written out as SentenceQualityEstimate into response.
  virtual void computeQualityScores(const Histories &histories, Response &response) const = 0;
};

/// Unsupervised Quality Estimator model. It uses the translator model's log probabilities (log probs) as a proxy for
/// quality scores. Then, for a given word, its quality score is computed by taking the mean of the log probs of the
/// tokens that make it up. The sentence score is the mean of all word's log probs.
class UnsupervisedQualityEstimator : public QualityEstimator {
 public:
  void computeQualityScores(const Histories &histories, Response &response) const override;

 private:
  Response::SentenceQualityScore computeSentenceScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                       const size_t sentenceIdx) const;
};

// ASCII and Unicode text files never start with the following 64 bits
// It serves as a signature for quality estimator binary files
constexpr std::uint64_t BINARY_QE_MODEL_MAGIC = 0x78cc336f1d54b180;

/// LogisticRegressorQualityEstimator model implementation through a linear regressor + sigmoid function. Simply
/// speaking, an LR model depends on features to be scaled, so it contains four elements of data: a vector of
/// coefficients and an intercept (which represents the linear model) and a vector of means and stds (which are
/// necessary for feature scaling). These variables are firstly initialized by parsing a file (which comes from
/// `fromAlignedMemory`), and then they are used to build a model representation
class LogisticRegressorQualityEstimator : public QualityEstimator {
 public:
  using Array = std::array<float, /*LRParamsDims = */ 4>;

  struct Header {
    /// Binary QE File magic number
    uint64_t magic;
    /// Length of lr parameters stds, means and coefficients.
    uint64_t lrParametersDims;
  };
  /// Struct that contains information for applying standard scaling
  struct Scale {
    /// Array of standard deviations of feature values. Its length will be equals as featureDims
    Array stds;
    /// Array of mean of feature values. Its length will be equals as featureDims
    Array means;
  };
  /// Matrix is an internal data structure that was created only to be used in LogisticRegressorQualityEstimator
  /// methods. It intends to represent a matrix, so it receives row and column values as a constructor. Furthermore, the
  /// method `at` can access specific data given a row and col position.
  class Matrix {
   public:
    /// Number of rows
    const size_t rows;
    /// Number of columns
    const size_t cols;

    /// @param [in] rowsParam: number of rows in the Matrix
    /// @param [in] colsParam: number of columns in the Matrix
    Matrix(const size_t rowsParam, const size_t colsParam);
    /// Move constructor
    Matrix(Matrix &&other);

    /// Return data value given a row and col position
    /// @param [in] row: row position
    /// @param [in] col: col position
    const float &at(const size_t row, const size_t col) const;
    float &at(const size_t row, const size_t col);

   private:
    std::vector<float> data_;
  };
  /// Logistic Regressor constructor. It creates a LR model that fits proper for the QualityEstimator use.
  ///
  ///
  /// @param [in] scale: Array of stds and means that can be used to apply standard scaling in the features
  /// @param [in] coefficients: coefficient values of linear part of LR model
  /// @param [in] intercept: intercept value of the linear part of LR model
  LogisticRegressorQualityEstimator(Scale &&scale, Array &&coefficients, const float intercept);

  /// Move constructor
  LogisticRegressorQualityEstimator(LogisticRegressorQualityEstimator &&other);

  /// Binary file parser which came from AlignedMemory
  /// It's expected from AlignedMemory the following structure:
  /// - -a header with the number of parameters dimensions
  /// - -a vector of standard deviations of features
  /// - -a vector of means of features
  /// - -a vector of coefficients
  /// - -a intercept value
  static LogisticRegressorQualityEstimator fromAlignedMemory(const AlignedMemory &alignedMemory);
  AlignedMemory toAlignedMemory() const;

  void computeQualityScores(const Histories &histories, Response &response) const override;
  /// Given an input matrix \f$\mathbf{X}\f$, the usual Logistic Regression calculus can be seen as the following:
  ///
  /// 1) Standardize it, returning in \f$\mathbf{Z} = \frac{(\mathbf{X}-\mu)}{\sigma}\f$, where \f$\mu\f$ stands for the
  /// mean vector and \f$\sigma\f$ represents the standard deviation
  ///
  /// 2) Then, we apply \f$\sum_{i=1}^{D}{ w_i z_i}\f$, where \f$D\f$ is the dimension (i.e. the number of features) and
  /// \f$w\f$ is the model vector with learnt weights
  ///
  /// 3) We apply the sigmoid function to the result
  ///
  /// Notice, however, that for the first two steps we can do the following:
  ///
  /// \f{align*}{
  /// \sum_{i=1}^{D}{ w_i z_i} &= \mathbf{w^T}\left(\mathbf{\sigma^{-1}} \odot (\mathbf{x} - \mathbf{\mu})\right) \text{
  /// //
  /// we are just vectoring step 1}\\
  ///      &= \sum_{i=1}^{D}{\sigma_i^{-1} w_i (x_i - \mu_i)} \\
  ///      &= \sum_{i=1}^{D}{\sigma_i^{-1} w_ix_i - \sigma_i^{-1} w_i \mu_i} \\
  ///      &= \sum_{i=1}^{D}{\left(\sigma_i^{-1} w_i\right)x_i - \left(\sigma_i^{-1} w_i \mu_i\right)}
  /// \f}
  /// Then, \f$(\sigma_i^{-1} w_i \mu_i)\f$ can be precomputed without any dependence on inference data. This is done by
  /// the variable \f$\textit{constantFactor_}\f$ and \f$\textit{intercept_}\f$ in the code.
  ///
  /// @param [in] features: A Matrix struct of features. For a defintion what features currently means, please refer to
  /// `extractFeatures` method in `quality_estimator.cpp`
  std::vector<float> predict(const Matrix &features) const;

 private:
  Scale scale_;
  Array coefficients_;
  float intercept_;
  Array coefficientsByStds_;
  float constantFactor_ = 0.0;

  // Number of parameters with dimension - Scale(stds, means) and coefficients
  static constexpr const size_t numLrParamsWithDimension_ = 3;
  // Number of intercept values
  static constexpr const size_t numIntercept_ = 1;

  /// construct the struct SentenceQualityEstimate
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  Response::SentenceQualityScore computeSentenceScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                       const size_t sentenceIdx) const;

  Matrix extractFeatures(const std::vector<SubwordRange> &wordIndices, const std::vector<float> &logProbs) const;
};

/// createQualityEstimator model takes an `AlignedMemory`, which is the return from `getQualityEstimatorModel`.
///
/// `getQualityEstimatorModel` contains two different implementations, one when the `quality` argument has some value as
/// a possible `Options` and where it does not.
///
/// If a non `quality` option is provided, then by default, it uses the UnsupervisedQualityEstimator implementation.
///
/// If a value is passed to the `quality` argument, the model file is read and converted into an `AlignedMemory`
/// structure, which instantiates a QualityEstimator object.

/// @param [in] qualityFileMemory: An `AlignedMemory` which is created by parsing a QE model binary file through
/// getQualityEstimatorModel
inline std::shared_ptr<QualityEstimator> createQualityEstimator(const AlignedMemory &qualityFileMemory) {
  // If no quality file return simple model
  if (qualityFileMemory.size() == 0) {
    return std::make_shared<UnsupervisedQualityEstimator>();
  }

  return std::make_shared<LogisticRegressorQualityEstimator>(
      LogisticRegressorQualityEstimator::fromAlignedMemory(qualityFileMemory));
}

/// A word is composed of multiple subtokens. Entire words are tokens splitted by whitespace.
/// This method takes a sequence of sublevel tokens (given by AnnotatedText) as well aligned with their log
/// probabilities and conflate them to their respective words
/// The return of this function is a SubwordRange (an alias of ByteRange) vector where each value corresponds to a word
/// id and its content represent the range of subword value that compose a given word
///
/// If a translated sentence does not contain any alphanumeric character (therefore, it is made basically of the EOS
/// token), this method ignores it and returns an empty ByteRange vector of words.
///
/// Examples:
/// Suppose that you have the following source target (A): marian is a good translation service and the translate
/// service gives you the following sentence (B):
/// service gives you the following sentence (B):
///
/// ma(0.15) ri(0.15) an(0.2) es(0.3) un(0.1) bu(0.3) en(0.2) ser(0.1) vi(0.2) cio(0.4) de(0.1) tra(0.4) du(0.2)
/// cción(0.1)
///
/// The numbers that the words follow represent the logProb of each BPE token.
///
/// Then, the result would be something like:
/// a vector where each position corresponds to the SubwordRange of the following words: marian
/// es un buen servicio de traducción. Hence, its length is 7. The value of the first element would be [0,3)

/// @param [in] logProbs: the log probabilities of byte pair encodings (BPE) that comes from the tracebackWordScores
/// method (which belongs to hypothesis.h in Marian)
/// @param [in] target: AnnotatedText target value
/// @param [in] sentenceIdx: the id of a candidate sentence
std::vector<SubwordRange> mapWords(const std::vector<float> &logProbs, const AnnotatedText &target,
                                   const size_t sentenceIdx);

}  // namespace marian::bergamot
