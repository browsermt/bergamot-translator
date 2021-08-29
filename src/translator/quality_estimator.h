#pragma once

#include <vector>

#include "annotation.h"
#include "matrix.h"
#include "response.h"
#include "translator/history.h"

namespace marian::bergamot {

/// Interface for quality estimator
class QualityEstimator {
 public:
  // Computes quality-scores using values from History and precomputed
  // and stored tokenizations within Response.
  //
  // @param [in] histories: Histories obtained from translating a blob of source-text
  // @param [inout] response: Partially constructed response, holding tokenization info
  // for source and target. The quality-scores for each sentence obtained from source-text blob
  // are written out as WordsQualityEstimate into response.
  virtual void computeQualityScores(const Histories &histories, Response &response) const = 0;
};

/// The UnsurpervisedQE stands for Unsurpervised Quality Estimator model. It basically uses the negative log
/// probabilities (logprobs) of the translator model as proxy for quality scores. Then, for a given word, it's quality
/// score is computed by taking the mean of the negative logprobs of the tokens that make up it. The sentence score is
/// the mean of all word's neg. logprob.
class UnsupervisedQualityEstimator : public QualityEstimator {
 public:
  void computeQualityScores(const Histories &histories, Response &response) const override;

 private:
  Response::WordsQualityEstimate computeSentenceScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                       const size_t sentenceIdx) const;
};

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
class LogisticRegressorQualityEstimator : public QualityEstimator {
 public:
  struct Header {
    uint64_t magic;             // BINARY_QE_MODEL_MAGIC
    uint64_t lrParametersDims;  // Length of lr parameters stds, means and coefficients .
  };

  struct Scale {
    std::vector<float> stds;
    std::vector<float> means;
  };

  LogisticRegressorQualityEstimator(Scale &&scale, std::vector<float> &&coefficients, const float intercept);

  LogisticRegressorQualityEstimator(LogisticRegressorQualityEstimator &&other);

  /// Binary file parser which came from AlignedMemory
  /// It's expected from AlignedMemory the following structure:
  /// - a header with the number of parameters dimensions
  /// - a vector of standard deviations of features
  /// - a vector of means of features
  /// - a vector of coefficients
  /// - a intercept value
  static LogisticRegressorQualityEstimator fromAlignedMemory(const AlignedMemory &alignedMemory);
  AlignedMemory toAlignedMemory() const;

  void computeQualityScores(const Histories &histories, Response &response) const override;

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

  static Matrix extractFeatures(const std::vector<std::vector<float>> &wordLogProbs);
};

/// The createQualityEstimator method create a quality estimator
/// By default, if the qualityFileMemory is empty it will use
/// the unsupervised learning approach (UnsupervisedQualityEstimator).
inline std::shared_ptr<QualityEstimator> createQualityEstimator(const AlignedMemory &qualityFileMemory) {
  // If no quality file return simple model
  if (qualityFileMemory.size() == 0) {
    return std::make_shared<UnsupervisedQualityEstimator>();
  }

  return std::make_shared<LogisticRegressorQualityEstimator>(
      LogisticRegressorQualityEstimator::fromAlignedMemory(qualityFileMemory));
}

std::pair<std::vector<std::vector<ByteRange>>, std::vector<std::vector<float>>> remapWordsAndLogProbs(
    const std::vector<float> &logProbs, const AnnotatedText &target, const size_t sentenceIdx);

std::vector<ByteRange> subwordToWords(const std::vector<std::vector<ByteRange>> &subwords);

}  // namespace marian::bergamot
