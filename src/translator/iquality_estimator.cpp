#include "iquality_estimator.h"

namespace marian::bergamot {

/// Interface for quality model

std::pair<std::vector<ByteRange>, std::vector<std::vector<float> > > IQualityEstimator::remapWords(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) {
  // Ignore empty target
  if ((logProbs.size() < 2) || (target.numWords(sentenceIdx) == 0)) {
    return {{}, {}};
  }

  std::vector<ByteRange> wordByteRanges;
  std::vector<std::vector<float> > wordlogProbs;

  // The first subword it's always a beginning of a word
  ByteRange subword = target.wordAsByteRange(sentenceIdx, 0);

  wordlogProbs.emplace_back().push_back(logProbs[0]);
  wordByteRanges.push_back(subword);

  /// A word is composed of multiple subtokens. The definition of an "entire"
  /// word is the presence of whitespace. The QE model ignores the presence
  /// of the EOS token, and hence we only need to iterate n-1 positions.
  for (size_t subwordIdx = 1; subwordIdx < (logProbs.size() - 1); ++subwordIdx) {
    subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    const char firstLetter = target.text.at(subword.begin);

    // if the first character is whitespace, it's a beginning of a new word
    if (isspace(firstLetter)) {
      wordlogProbs.emplace_back().push_back(logProbs[subwordIdx]);
      ++subword.begin;
      wordByteRanges.push_back(subword);
    } else {
      // update last word byte range and word features
      wordlogProbs.back().push_back(logProbs[subwordIdx]);
      ByteRange& currentWord = wordByteRanges.back();

      currentWord.end = subword.end;
    }
  }

  return {wordByteRanges, wordlogProbs};
}
}  // namespace marian::bergamot
