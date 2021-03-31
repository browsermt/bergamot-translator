#include "sentence_ranges.h"
#include <cassert>
#include <iostream>

namespace marian {
namespace bergamot {

void Annotation::addSentence(std::vector<ByteRange> &sentence) {
  size_t size = flatByteRanges_.size();
  flatByteRanges_.insert(std::end(flatByteRanges_), std::begin(sentence),
                         std::end(sentence));
  sentenceBeginIds_.push_back(size);
}

size_t Annotation::numWords(size_t sentenceIdx) const {
  auto terminals = sentenceTerminalIds(sentenceIdx);
  return terminals.second - terminals.first + 1;
}

std::pair<size_t, size_t>
Annotation::sentenceTerminalIds(size_t sentenceIdx) const {
  size_t bosId, eosId;
  bosId = sentenceBeginIds_[sentenceIdx];
  eosId = sentenceIdx + 1 < numSentences()
              ? sentenceBeginIds_[sentenceIdx + 1] - 1
              : flatByteRanges_.size() - 1;

  // Out of bound checks.
  assert(bosId < flatByteRanges_.size());
  assert(eosId < flatByteRanges_.size());
  return std::make_pair(bosId, eosId);
}

std::pair<ByteRange, ByteRange>
Annotation::sentenceTerminals(size_t sentenceIdx) const {
  auto terminals = sentenceTerminalIds(sentenceIdx);
  return std::make_pair(flatByteRanges_[terminals.first],
                        flatByteRanges_[terminals.second]);
}

ByteRange Annotation::sentence(size_t sentenceIdx) const {
  auto terminals = sentenceTerminals(sentenceIdx);
  return (ByteRange){terminals.first.begin, terminals.second.end};
}

ByteRange Annotation::word(size_t sentenceIdx, size_t wordIdx) const {
  size_t offset = sentenceBeginIds_[sentenceIdx];
  // auto terminals = sentenceTerminals(sentenceIdx);
  // assert(offset + wordIdx <= terminals.second);
  return flatByteRanges_[offset + wordIdx];
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
        (ByteRange){thisWordBegin, thisWordBegin + wordView.size()});
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
