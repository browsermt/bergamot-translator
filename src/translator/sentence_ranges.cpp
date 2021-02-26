#include "sentence_ranges.h"
#include <cassert>
#include <iostream>

namespace marian {
namespace bergamot {

void SentenceRanges::addSentence(std::vector<string_view> &wordRanges) {
  addSentence(std::begin(wordRanges), std::end(wordRanges));
}

void SentenceRanges::addSentence(WordIterator begin, WordIterator end) {
  size_t size = flatByteRanges_.size();
  flatByteRanges_.insert(std::end(flatByteRanges_), begin, end);
  sentenceBeginIds_.push_back(size);
}

string_view SentenceRanges::sentence(size_t index) const {
  size_t bos_id;
  string_view eos, bos;

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

string_view SentenceRanges::sentenceBetween(string_view firstWord,
                                            string_view lastWord) const {

  const char *data = firstWord.data();
  size_t size = lastWord.data() + lastWord.size() - firstWord.data();
  return string_view(data, size);
}

} // namespace bergamot
} // namespace marian
