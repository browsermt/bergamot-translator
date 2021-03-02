#include "sentence_ranges.h"
#include <cassert>
#include <iostream>

namespace marian {
namespace bergamot {

template <class string_view_type>
void SentenceRangesT<string_view_type>::addSentence(
    std::vector<string_view_type> &wordRanges) {
  addSentence(std::begin(wordRanges), std::end(wordRanges));
}

template <class string_view_type>
void SentenceRangesT<string_view_type>::addSentence(WordIterator begin,
                                                    WordIterator end) {
  size_t size = flatByteRanges_.size();
  flatByteRanges_.insert(std::end(flatByteRanges_), begin, end);
  sentenceBeginIds_.push_back(size);
}

template <class string_view_type>
string_view_type
SentenceRangesT<string_view_type>::sentence(size_t index) const {
  size_t bos_id;
  string_view_type eos, bos;

  bos_id = sentenceBeginIds_[index];
  bos = flatByteRanges_[bos_id];

  if (index + 1 == numSentences()) {
    eos = flatByteRanges_.back();
  } else {
    assert(index < numSentences());
    size_t eos_id = sentenceBeginIds_[index + 1];
    --eos_id;
    eos = flatByteRanges_[eos_id];
  }

  return sentenceBetween(bos, eos);
}

template <class string_view_type>
string_view_type SentenceRangesT<string_view_type>::sentenceBetween(
    string_view_type firstWord, string_view_type lastWord) const {

  const char *data = firstWord.data();
  size_t size = lastWord.data() + lastWord.size() - firstWord.data();
  return string_view_type(data, size);
}

} // namespace bergamot
} // namespace marian
