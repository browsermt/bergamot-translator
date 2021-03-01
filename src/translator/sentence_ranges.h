#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include "data/types.h"
#include <cassert>
#include <vector>

namespace marian {
namespace bergamot {

template <class string_view_type> class SentenceRangesT {
  // SentenceRanges stores string_view_types into a source text, with additional
  // annotations to mark sentence boundaries.
  //
  // Given the availability annotations, this container provides capabilty to
  // add sentences, and access individual sentences.
public:
  typedef std::vector<string_view_type>::iterator WordIterator;

  void addSentence(std::vector<string_view_type> &wordRanges);
  void addSentence(WordIterator begin, WordIterator end);

  void clear() {
    flatByteRanges_.clear();
    sentenceBeginIds_.clear();
  }

  size_t numSentences() const { return sentenceBeginIds_.size(); }

  // Returns a string_view_type into the ith sentence.
  string_view_type sentence(size_t index) const;

private:
  // A flat storage for string_view_types. Can be words or sentences.
  std::vector<string_view_type> flatByteRanges_;

  // The container grows dynamically with addSentence. size_t marking index is
  // used to ensure the sentence boundaries stay same while underlying storage
  // might be changed during reallocation.
  std::vector<size_t> sentenceBeginIds_;

  // Utility function to extract the string starting at firstWord and ending at
  // lastWord as a single string-view.
  string_view_type sentenceBetween(string_view_type firstWord,
                                   string_view_type lastWord) const;
};

typedef SentenceRangesT<string_view> SentenceRanges;

} // namespace bergamot

} // namespace marian

#endif //  BERGAMOT_SENTENCE_RANGES_H_
