#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <vector>

namespace marian {
namespace bergamot {

/// SentenceRanges stores `string_view`s into a source text, with additional
/// annotations to mark sentence boundaries. This class enables efficient
/// storage with a flat string_view vector, while providing a convenient API to
/// access the i-th sentences.
///
/// Given the availability annotations, this container provides capabilty to
/// add sentences, and access individual sentences.
//
class SentenceRanges {
public:
  typedef std::vector<string_view>::iterator WordIterator;

  void addSentence(std::vector<string_view> &wordRanges);
  void addSentence(WordIterator begin, WordIterator end);

  void clear() {
    flatByteRanges_.clear();
    sentenceBeginIds_.clear();
  }

  size_t numSentences() const { return sentenceBeginIds_.size(); }

  /// Returns a string_view into the i-th sentence.
  string_view sentence(size_t index) const;

private:
  /// A flat storage for string_views. Can be words or sentences.
  std::vector<string_view> flatByteRanges_;

  /// The container grows dynamically with addSentence. size_t marking index is
  /// used to ensure the sentence boundaries stay same while underlying storage
  /// might be changed during reallocation.
  std::vector<size_t> sentenceBeginIds_;

  /// Utility function to extract the string starting at firstWord and ending at
  /// lastWord as a single string-view.
  string_view sentenceBetween(string_view firstWord,
                              string_view lastWord) const;
};

} // namespace bergamot
} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
