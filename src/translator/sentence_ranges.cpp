#include "sentence_ranges.h"
#include <cassert>
#include <iostream>

namespace marian {
namespace bergamot {

void Annotation::addSentence(std::vector<ByteRange> &sentence) {
  flatByteRanges_.insert(std::end(flatByteRanges_), std::begin(sentence),
                         std::end(sentence));
  size_t size = flatByteRanges_.size();
  sentenceEndIds_.push_back(size);
}

size_t Annotation::numWords(size_t sentenceIdx) const {
  size_t bosId, eosId;
  bosId = sentenceEndIds_[sentenceIdx]; // Half interval, so;
  eosId = sentenceEndIds_[sentenceIdx + 1];
  // Difference between eosId and bosId is the number of words.
  return eosId - bosId;
}

ByteRange Annotation::sentence(size_t sentenceIdx) const {
  size_t bosId, eosId;
  bosId = sentenceEndIds_[sentenceIdx]; // Half interval, so;
  eosId = sentenceEndIds_[sentenceIdx + 1];
  ByteRange sentenceByteRange;

  if (bosId == eosId) {
    // We have an empty sentence. However, we want to be able to point where in
    // target this happened through the ranges. We are looking for the end of
    // the flatByteRange and non-empty sentence before this happened and
    // construct empty string-view equivalent ByteRange.
    ByteRange eos = flatByteRanges_[eosId - 1];
    sentenceByteRange = ByteRange{eos.end, eos.end};
  } else {
    ByteRange bos = flatByteRanges_[bosId];
    ByteRange eos = flatByteRanges_[eosId - 1];
    sentenceByteRange = ByteRange{bos.begin, eos.end};
  }
  return sentenceByteRange;
}

ByteRange Annotation::word(size_t sentenceIdx, size_t wordIdx) const {
  size_t bosOffset = sentenceEndIds_[sentenceIdx];
  return flatByteRanges_[bosOffset + wordIdx];
}

string_view AnnotatedText::word(size_t sentenceIdx, size_t wordIdx) const {
  auto terminals = annotation.word(sentenceIdx, wordIdx);
  return string_view(&text[terminals.begin], terminals.size());
}

string_view AnnotatedText::sentence(size_t sentenceIdx) const {
  auto sentenceAsByteRange = annotation.sentence(sentenceIdx);
  return asStringView(sentenceAsByteRange);
}

void AnnotatedText::appendSentence(std::string prefix, std::string &reference,
                                   std::vector<string_view> &wordRanges) {
  text += prefix;
  size_t offset = text.size(); // Get size before to do ByteRange arithmetic
  text += reference;           // Append reference to text
  std::vector<ByteRange> sentence;
  for (auto &wordView : wordRanges) {
    size_t thisWordBegin = offset + wordView.data() - &reference[0];
    sentence.push_back(
        ByteRange{thisWordBegin, thisWordBegin + wordView.size()});
  }
  annotation.addSentence(sentence);
}

void AnnotatedText::addSentence(std::vector<string_view> &wordRanges) {
  addSentence(std::begin(wordRanges), std::end(wordRanges));
};

void AnnotatedText::addSentence(std::vector<string_view>::iterator begin,
                                std::vector<string_view>::iterator end) {
  std::vector<ByteRange> sentence;
  for (auto p = begin; p != end; p++) {
    size_t begin_offset = p->data() - &text[0];
    sentence.push_back(ByteRange{begin_offset, begin_offset + p->size()});
  }
  annotation.addSentence(sentence);
};

ByteRange AnnotatedText::wordAsByteRange(size_t sentenceIdx,
                                         size_t wordIdx) const {
  return annotation.word(sentenceIdx, wordIdx);
}

ByteRange AnnotatedText::sentenceAsByteRange(size_t sentenceIdx) const {
  return annotation.sentence(sentenceIdx);
}

string_view AnnotatedText::asStringView(const ByteRange &byteRange) const {
  const char *data = &text[byteRange.begin];
  size_t size = byteRange.size();
  return string_view(data, size);
}

} // namespace bergamot
} // namespace marian
