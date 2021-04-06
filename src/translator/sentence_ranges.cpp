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
  auto terminals = sentenceTerminalIds(sentenceIdx);
  return terminals.second - terminals.first;
}

std::pair<size_t, size_t>
Annotation::sentenceTerminalIds(size_t sentenceIdx) const {
  size_t bosId, eosId;
  bosId = (sentenceIdx == 0)
              ? 0                                 // Avoid -1 access
              : sentenceEndIds_[sentenceIdx - 1]; // Half interval, so;

  eosId = sentenceEndIds_[sentenceIdx];
  return std::make_pair(bosId, eosId);
}

ByteRange Annotation::sentence(size_t sentenceIdx) const {
  auto terminals = sentenceTerminalIds(sentenceIdx);
  auto bos = flatByteRanges_[terminals.first];
  auto eos = flatByteRanges_[terminals.second - 1];
  return (ByteRange){bos.begin, eos.end};
}

ByteRange Annotation::word(size_t sentenceIdx, size_t wordIdx) const {
  size_t bosOffset = (sentenceIdx == 0) ? 0 : sentenceEndIds_[sentenceIdx - 1];
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

void AnnotatedText::addSentence(std::vector<string_view> &wordRanges) {
  addSentence(std::begin(wordRanges), std::end(wordRanges));
};

void AnnotatedText::addSentence(std::vector<string_view>::iterator begin,
                                std::vector<string_view>::iterator end) {
  std::vector<ByteRange> sentence;
  for (auto p = begin; p != end; p++) {
    size_t begin_offset = p->data() - &text[0];
    sentence.push_back((ByteRange){begin_offset, begin_offset + p->size()});
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
