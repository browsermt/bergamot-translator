#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "annotation.h"
#include "definitions.h"
#include "iquality_model.h"
#include "marian.h"
#include "response.h"
#include "translator/history.h"

namespace marian::bergamot {
/// QualityEstimator (QE) is responsible for measuring the quality of a translation model.
/// It returns the probability of each translated term being a valid one.
/// It's worthwhile mentioning that a word is made of one or more byte pair encoding (BPE) tokens.
class QualityEstimator {
 public:
  /// Construct a QualityEstimator
  /// @param [in] logisticRegressor:
  explicit QualityEstimator(const std::shared_ptr<IQualityModel> &model);

  QualityEstimator(QualityEstimator &&other);

  /// construct the struct WordsQualityEstimate
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  Response::WordsQualityEstimate computeQualityScores(const std::vector<float> &logProbs, const AnnotatedText &target,
                                                      const size_t sentenceIdx) const;

 private:
  /// Builds the words byte ranges and defines the features
  /// @param [in] logProbs: the log probabilities given by an translation model
  /// @param [in] target: AnnotatedText target value
  /// @param [in] sentenceIdx: the id of a candidate sentence
  /// This method generates the following features:
  /// - the mean BPE log Probabilities of a given word.
  /// - the minimum BPE log Probabilities of a given word.
  /// - the number of BPE tokens that a word is made of
  /// - the overall mean considering all BPE tokens in a sentence
  static std::pair<std::vector<ByteRange>, Matrix> remapWordsAndExtractFeatures(const std::vector<float> &logProbs,
                                                                                const AnnotatedText &target,
                                                                                const size_t sentenceIdx);

  std::shared_ptr<IQualityModel> model_;
};

}  // namespace marian::bergamot
