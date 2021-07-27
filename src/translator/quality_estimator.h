#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <string>
#include <vector>

#include "annotation.h"
#include "definitions.h"
#include "intgemm/intgemm.h"

namespace marian {
namespace bergamot {
// ASCII and Unicode text files never start with the following 64 bits
constexpr std::size_t BINARY_QE_MODEL_MAGIC = 0x78cc336f1d54b180;

/// QualityEstimator (QE) is responsible for measuring the quality of a translation model.
/// It returns the probability of each translated term being a valid one.
/// It's worthwhile mentioning that a word is made of one or more byte pair encoding (BPE) tokens.

/// Currently, it only expects an AlignedMemory, which is given from a binary file,
/// which contains a header.
/// The header includes the following structure:
/// - a vector of standard deviations of features
/// - a vector of means of features
/// - a vector of coefficients
/// - a intercept value
///
/// Where each feature corresponds to one of the following:
/// - the mean BPE log Probabilities of a given word.
/// - the minimum BPE log Probabilities of a given word.
/// - the number of BPE tokens that a word is made of
/// - the overall mean considering all BPE tokens in a sentence

/// The current model implementation is a Logistic Model, so after the matrix multiply,
// there is a non-linear sigmoid transformation that converts the final scores into probabilities.
class QualityEstimator {
 public:
  struct Header {
    uint64_t magic;             // BINARY_QE_MODEL_MAGIC
    uint64_t lrParametersDims;  // Length of lr parameters stds, means and coefficients .
  };

  /// WordsQualityEstimate contains the quality data of a given translated sentence.
  /// It includes the confidence (proxied by a probability) of each decoded word
  /// (higher probabilities imply better-translated words), the ByteRanges of each term,
  /// and the probability of the whole sentence, represented as the mean word scores.
  struct WordsQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore = 0.0;
  };

  /// Construct a QualityEstimator
  /// @param [in] qualityEstimatorMemory: AlignedMemory built from quality estimator binary file
  explicit QualityEstimator(const AlignedMemory &qualityEstimatorMemory);

  /// construct the struct WordsQualityEstimate
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  WordsQualityEstimate computeQualityScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                            const size_t sentenceIdx) const;

  AlignedMemory toAlignedMemory() const;

 private:
  struct Matrix {
    Matrix(const size_t rowsParam, const size_t widthParam);

    const float &at(const size_t row, const size_t col) const;
    float &at(const size_t row, const size_t col);

    const size_t rows;
    const size_t cols;
    AlignedVector<float> data;
  };

  struct IntgemmMatrix : Matrix {
    IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index widthParam, const intgemm::Index rowsMultiplier,
                  const intgemm::Index widthMultiplier);

     Matrix operator*( const IntgemmMatrix& matrixb ) const;
  };

  struct Scale {
    std::vector<float> stds;
    std::vector<float> means;
  };

  /// WordFeatures represents the features used by a given model.
  ///
  /// It's valued are filled through mapBPEToWords
  struct WordFeatures {
    int numSubWords = 0;
    float meanScore = 0.0;
    float minScore = 0.0;
  };

  /// The current Quality Estimator model is a Logistic Model implemented through
  /// a linear regressor + sigmoid function. Simply speaking, an LR model depends
  /// on features to be scaled, so it contains four elements of data: a vector of coefficients
  /// and an intercept (which represents the linear model) and a vector of means and stds
  /// (which are necessary for feature scaling).
  ///
  /// These pointers are firstly initialized by parsing a file (which comes from memory),
  /// and then they are used to build a model representation (which is a matrix)
  class LogisticRegressor {
   public:
    const Scale scale;
    const IntgemmMatrix coefficients;
    const size_t numCoefficients;
    const float intercept;

    LogisticRegressor(Scale &&scaleParam, IntgemmMatrix &&coefficientsParam, const size_t numCoefficientsParam,
                      const float interceptParam);

    std::vector<float> predict(const Matrix &features) const;

   private:
    /// Calculates the scores of words through a linear model
    /// @param [in] features: the matrix of feature scaled values
    std::vector<float> vectorResult(const IntgemmMatrix &features) const;

    /// Given the modelFeatures construct, it builds the feature matrix with scaled values
    /// @param [in] feature: a struct matrix which contains the std and mean vectors of each feature
    /// @param [in] overallMean: a float record that represents the mean of sentence bpe tokens logprobs
    IntgemmMatrix transformFeatures(const Matrix &features) const;

    /// Applies a sigmoid function to each element of a vector and returns the mean of the result vector
    /// @param [in] linearPredictedValues: the vector of real values returned by a linear regression
    void resultToProbabilities(std::vector<float> &linearPredictedValues) const;
  };

  /// binary file parser which came from AlignedMemory
  static LogisticRegressor fromAlignedMemory(const AlignedMemory &qualityEstimatorMemory);

  /// Builds the words byte ranges and defines the WordFeature values
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  static std::tuple<std::vector<ByteRange>, std::vector<WordFeatures>, float> mapBPEToWords(
      const std::vector<float> &logProbs, const AnnotatedText &target, const size_t sentenceIdx);

  static Matrix convertToMatrix(const std::vector<WordFeatures> &wordsFeatures, const float overallMean);

  LogisticRegressor logisticRegressor_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
