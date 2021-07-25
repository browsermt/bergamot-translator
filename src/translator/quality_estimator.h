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

/// QualityEstimator is responsible for measuring the quality of a translation
/// model.
///
/// Currently, it only expects an AlignedMemory, which should be provided
/// from a binary file, which contains a header.
///
/// The header contains the following structure: a vector of standard deviations
/// of features; a vector of means of features; a vector of coefficients
/// and a vector of constant, which correspond to the intercept.
///
/// The first two ones are necessary for feature scaling whereas the last two are necessary
/// for model computation.
///
/// The current model implementation is a Logistic Model, so after the matrix multiply,
/// there is a non-linear sigmoid transformation which converts the final scores
/// into probabilities.
///
/// The model takes four features: the mean of a word byte pair encoding log probabilities;
/// the minimum of logprobs; the number of bpe that a word is made of and the overall mean
/// of bpe tokens log probs
class QualityEstimator {
 public:
  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };

  /// WordQualityScores contains the quality data of a given translated sentence
  ///
  /// It contains the confidence of each translated word quality
  /// (higher probabilities implies in better translated words); the ByteRanges of each word
  /// and the confidence of the whole sentence, represented as the mean word scores
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

 private:
  struct IntgemmMatrix {
    IntgemmMatrix(const intgemm::Index rowsParam, const intgemm::Index widthParam, const intgemm::Index rowsMultiplier,
                  const intgemm::Index widthMultiplier);

    intgemm::Index rows;
    intgemm::Index cols;
    AlignedVector<float> data;
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
  /// a linear regressor + sigmoid function. Simply speaking, a LR model depends on
  /// features to be scaled, so it contains four elements of data: a vector of coefficients
  /// and a intercept (which represents the linear model) and a vector os means and stds
  /// (which are necessary for feature scaling). These pointers are firstly initialized by
  /// parsing a file (which comes from memory) and then they are used to build a model
  /// representation (which is a matrix)
  class LogisticRegressor {
   public:
    enum Configuration { NumberOfFeatures = 4, ParametersDims = 4, CoefficientsColumnSize = 1 };

    LogisticRegressor(Scale &&scale, IntgemmMatrix &&coefficients, const float intercept);

    WordsQualityEstimate predictQualityScores(const std::vector<ByteRange> &wordByteRanges,
                                              const std::vector<WordFeatures> &wordsFeatures,
                                              const float overallMean) const;

   private:
    /// Calculates the scores of words through a linear model
    /// @param [in] featureMatrix: the matrix of feature scaled values
    std::vector<float> vectorResult(const IntgemmMatrix &features) const;

    /// Given the modelFeatures construct, it builds the feature matrix with scaled values
    /// @param [in] wordFeatures: a struct which contains the std and mean vectors of each feature
    /// @param [in] overallMean: a float record that represents the mean of sentence bpe tokens logprobs
    IntgemmMatrix transformFeatures(const std::vector<WordFeatures> &wordsFeatures, const float overallMean) const;

    /// Applies a sigmoid function to each element of a vector and returns the mean of the result vector
    /// @param [in] linearPredictedValues: the vector of real values returned by a linear regression
    float resultToProbabilities(std::vector<float> &linearPredictedValues) const;

    const Scale scale_;
    const IntgemmMatrix coefficients_;
    const float intercept_;
  };

  /// binary file parser which came from AlinedMemory
  static LogisticRegressor load(const AlignedMemory &qualityEstimatorMemory);

  /// Builds the words byte ranges (including EOS token) and defines the ModelFeatures values
  /// @param [in] lobProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  std::tuple<std::vector<ByteRange>, std::vector<WordFeatures>, float> mapBPEToWords(const std::vector<float> &logProbs,
                                                                                     const AnnotatedText &target,
                                                                                     const size_t sentenceIdx) const;

  LogisticRegressor logisticRegressor_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
