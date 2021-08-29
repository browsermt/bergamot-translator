#include "iquality_estimator.h"

namespace marian::bergamot {

std::pair<std::vector<std::vector<ByteRange>>, std::vector<std::vector<float>>> remapWordsAndLogProbs(
    const std::vector<float>& logProbs, const AnnotatedText& target, const size_t sentenceIdx) {
  // Ignore empty target
  if ((logProbs.size() < 2) || (target.numWords(sentenceIdx) == 0)) {
    return {{}, {}};
  }

  std::vector<std::vector<ByteRange>> wordByteRanges(/*numWords=*/1);
  std::vector<std::vector<float>> wordlogProbs(/*numWords=*/1);

  /// A word is composed of multiple subtokens. The definition of an "entire"
  /// word is the presence of whitespace. The QE model ignores the presence
  /// of the EOS token, and hence we only need to iterate n-1 positions.
  for (size_t subwordIdx = 0; subwordIdx < (logProbs.size() - 1); ++subwordIdx) {
    ByteRange subword = target.wordAsByteRange(sentenceIdx, subwordIdx);

    const char firstLetter = target.text.at(subword.begin);

    // if the first character is whitespace, it's a beginning of a new word
    if (isspace(firstLetter)) {
      wordlogProbs.emplace_back();
      wordByteRanges.emplace_back();
      ++subword.begin;
    }

    // update last word byte range and word features
    wordlogProbs.back().push_back(logProbs[subwordIdx]);
    wordByteRanges.back().push_back(subword);
  }

  return {wordByteRanges, wordlogProbs};
}
}  // namespace marian::bergamot
