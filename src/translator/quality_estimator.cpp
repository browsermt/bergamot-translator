#include "quality_estimator.h"

#include <iostream>
#include <numeric>

#include "byte_array_util.h"

namespace marian::bergamot {

QualityEstimator::QualityEstimator(LogisticRegressor&& logisticRegressor)
    : logisticRegressor_(std::move(logisticRegressor)) {}

QualityEstimator QualityEstimator::fromAlignedMemory(const AlignedMemory& qualityEstimatorMemory) {
  return QualityEstimator(LogisticRegressor::fromAlignedMemory(qualityEstimatorMemory));
}

QualityEstimator::QualityEstimator(QualityEstimator&& other)
    : logisticRegressor_(std::move(other.logisticRegressor_)) {}

AlignedMemory QualityEstimator::toAlignedMemory() const { return logisticRegressor_.toAlignedMemory(); }

void QualityEstimator::operator()(const Histories& histories, Response& response) const {
  for (const auto& history : histories) {
    const auto logProbs = std::get<1>(history->top())->tracebackWordScores();

    for (size_t s = 0; s < response.target.numSentences(); ++s) {
      response.qualityScores.push_back(computeQualityScores(logProbs, response.target, s));
    }
  }
}

// mapBPEToWords takes the following arguments:
// - the log probabilities (logProbs) of byte pair encodings (BPE)
//   that comes from the tracebackWordScores method (which belongs to hypothesis.h in Marian)
// - the AnnotatedText from the translated word
// - the index of a translated sentence
//
// This method is responsible for mapping BPE tokens to word tokens representing them through ByteRanges.
//
// The words byte ranges are expected to be alphanumeric characters, and they are split using whitespaces. Moreover,
// this method is also responsible for extracting the features that the QE model will further use. The features
// extracted are the following:
//
// - meanScore = the mean of bpe's  logProbs  that a given word corresponds to
// - minScore = the minimum bpe's logProbs that a given word corresponds to
// - numSubWords = the number of bpe tokens that a term is made of
// - overallMean = the mean of bpe's logProbs regarding the whole translated sentence
//
// The return of this function is a ByteRange vector of the words and a WordFeatures vector.
//
// If a translated sentence does not contain any alphanumeric character (therefore, it is made basically of the EOS
// token), this method ignores it and returns an empty ByteRange vector of words and an empty WordFeatures vector.
//
// Examples:
// Suppose that you have the following source target (A): marian is a good translation service and the translate
// service gives you the following sentence (B):
//
// ma(0.15) ri(0.15) an(0.2) es(0.3) un(0.1) bu(0.3) en(0.2) ser(0.1) vi(0.2) cio(0.4) de(0.1) tra(0.4) du(0.2)
// cción(0.1)
//
// The numbers that the words follow represent the logProb of each BPE token.
//
// Then, the result would be something like:
// a vector where each position corresponds to the ByteRanges of the following words: marian es un buen servicio de
// traducción. Hence, its length is 7.
//
// An vector of WordFeatures with length 7 where, for instance:
//
// the values of the first element (marian) would be:
// - meanScores= (0.15+0.15+0.3)/3=0.2
// - minScores= 0.15
// - numSubWords = 3
// - overallMean = 0.207
//
// the values of the second element (es) would be:
// - meanScores= 0.3
// - minScores= 0.3
// - numSubWords = 1
// - overallMean = 0.207
//
std::pair<std::vector<ByteRange>, Matrix> QualityEstimator::remapWordsAndExtractFeatures(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) {
  // Ignore empty target
  if ((logProbs.size() < 2) || (target.numWords(sentenceIdx) == 0)) {
    return {{}, Matrix(0, 0)};
  }

  const string_view sentence = target.sentence(sentenceIdx);
  const size_t numWords = std::count(std::begin(sentence), std::end(sentence), ' ') + 1;
  Matrix features(numWords, /*numFeatures =*/4);
  const size_t I_MEAN{0}, I_MIN{1}, I_NUM_SUBWORDS{2}, I_OVERALL_MEAN{3};

  std::vector<ByteRange> wordByteRanges;

  // The first subword it's always a beginning of a word
  float subwordScore = logProbs[0];
  ByteRange subword = target.wordAsByteRange(sentenceIdx, 0);

  float sequence = subwordScore;

  size_t featureRow = 0;

  features.at(featureRow, I_MEAN) = subwordScore;
  features.at(featureRow, I_MIN) = subwordScore;
  features.at(featureRow, I_NUM_SUBWORDS) = 1;

  wordByteRanges.push_back(subword);

  size_t subwordIdx = 1;
  /// A word is composed of multiple subtokens. The definition of an "entire"
  /// word is the presence of whitespace. The QE model ignores the presence
  /// of the EOS token, and hence we only need to iterate n-1 positions.
  for (; subwordIdx < (logProbs.size() - 1); ++subwordIdx) {
    const float subwordScore = logProbs[subwordIdx];
    sequence += subwordScore;

    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    const char firstLetter = target.text.at(subword.begin);

    // if the first character is whitespace, it's a beginning of a new word
    if (isspace(firstLetter)) {
      ++subword.begin;
      ++featureRow;
      features.at(featureRow, I_MEAN) = subwordScore;
      features.at(featureRow, I_MIN) = subwordScore;
      features.at(featureRow, I_NUM_SUBWORDS) = 1;

      wordByteRanges.push_back(subword);
    } else {
      // update last word byte range and word features

      ByteRange& currentWord = wordByteRanges.back();

      float& meanScore = features.at(featureRow, I_MEAN);
      float& minScore = features.at(featureRow, I_MIN);
      float& numSubWords = features.at(featureRow, I_NUM_SUBWORDS);

      currentWord.end = subword.end;
      ++numSubWords;
      // incremental mean
      meanScore += (subwordScore - meanScore) / numSubWords;

      if (minScore > subwordScore) {
        minScore = subwordScore;
      }
    }
  }

  const float overallMean = sequence / subwordIdx;

  for (int i = 0; i < features.rows; ++i) {
    features.at(i, I_OVERALL_MEAN) = overallMean;
  }

  return {wordByteRanges, std::move(features)};
}

Response::WordsQualityEstimate QualityEstimator::computeQualityScores(const std::vector<float>& logProbs,
                                                                      const AnnotatedText& target,
                                                                      const size_t sentenceIdx) const {
  const auto [wordByteRanges, features] = remapWordsAndExtractFeatures(logProbs, target, sentenceIdx);

  const auto wordQualityScores = logisticRegressor_.predict(features);

  const auto sentenceScore = std::accumulate(std::begin(wordQualityScores), std::end(wordQualityScores), float(0.0)) /
                             wordQualityScores.size();

  return {wordQualityScores, wordByteRanges, sentenceScore};
}
}  // namespace marian::bergamot
