#ifndef SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#define SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
#include <iostream>
#include <string>
#include <vector>

#include "annotation.h"
#include "intgemm/intgemm.h"
#include "response.h"

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
///The current model implementation is a Logistic Model, so after the matrix multiply, 
/// there is a non-linear sigmoid transformation which converts the final scores
/// into probabilities.
///
/// The model takes four features: the mean of a word byte pair encoding log probabilities; 
/// the minimum of logprobs; the number of bpe that a word is made of and the overall mean
/// of bpe tokens log probs

class QualityEstimator {
 private:
  AlignedMemory memory_;
  const float *stds_ = nullptr;
  const float *means_ = nullptr;
  const float *coefficients_ = nullptr;
  const float *intercept_ = nullptr;
  int numFeatures_ = 0;
  AlignedVector<float> modelMatrix_;

  void load(const char *ptr_void, const size_t blobSize);

 public:
  struct Header {
    uint64_t magic;        // BINARY_QE_MODEL_MAGIC
    uint64_t numFeatures;  // Length of all arrays.
  };

/// WordQualityScores contains the quality data of a given translated sentence
/// 
/// It contains the confidence of each translated word quality 
/// (bigger probabilities implies in better models); the ByteRanges of each word
/// the mean confidence, which represents the mean confidence.
///
/// This is going to be the result of computeQualityScores
  struct WordsQualityEstimate {
    std::vector<float> wordQualityScores;
    std::vector<ByteRange> wordByteRanges;
    float sentenceScore = 0.0;
  };

/// ModelFeatures represents the features used by a given model.
/// 
/// It's valued are filled through mapBPEToWords
  struct ModelFeatures {
    std::vector<float> wordMeanScores;
    std::vector<float> minWordScores;
    std::vector<int> numSubWords;
    float overallMean = 0.0;
  };

  /// Construct a QualityEstimator
  /// @param [in] qualityEstimatorMemory: AlignedMemory built from quality estimator binary file
  explicit QualityEstimator(AlignedMemory &&qualityEstimatorMemory);

  /// Builds the words byte ranges (including EOS token) and defines the ModelFeatures values
  /// @param [in] lobProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  std::pair<std::vector<ByteRange>, ModelFeatures> mapBPEToWords(const std::vector<float> &logProbs,
                                                                 const AnnotatedText &target,
                                                                 const size_t sentenceIdx) const;
  
  /// Calculates the scores of words through a linear model
  /// @param [in] featureMatrix: the matrix of feature scaled values 
  /// @param [in] numWords: the total number of words, including EOS token
  std::vector<float> predictWordScores(const AlignedVector<float> &featureMatrix, const int numWords) const;

  /// Defines a vector which correspond to a Linear Regression model
  AlignedVector<float> buildLinearModel() const;

  /// Given the modelFeatures construct, it builds the feature matrix with scaled values
  /// @param [in] modelFeatures: a struct which contains the std and mean vectores of each feature
  AlignedVector<float> extractFeatures(const ModelFeatures &modelFeatures) const;

  /// Applies a sigmoid function to each word in a vector and returns the mean of these probabilities
  /// @param [in] wordQualityScores: the vector of real values returned by a linear regression
  float computeWordProbabilities(std::vector<float> &wordQualityScores) const;

  /// construct the struct WordsQualityEstimate
  /// @param [in] lobProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  WordsQualityEstimate computeQualityScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                            const size_t sentenceIdx) const;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_QUALITY_ESTIMATOR_H_
