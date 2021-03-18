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
  return (ByteRange){terminals.first.begin_byte_offset,
                     terminals.second.end_byte_offset};
}

ByteRange Annotation::word(size_t sentenceIdx, size_t wordIdx) const {
  size_t offset = sentenceBeginIds_[sentenceIdx];
  // auto terminals = sentenceTerminals(sentenceIdx);
  // assert(offset + wordIdx <= terminals.second);
  return flatByteRanges_[offset + wordIdx];
}

string_view AnnotatedBlob::word(size_t sentenceIdx, size_t wordIdx) const {
  auto terminals = annotation.word(sentenceIdx, wordIdx);
  return string_view(&blob[terminals.begin_byte_offset], terminals.size());
}

string_view AnnotatedBlob::sentence(size_t sentenceIdx) const {
  auto sentenceAsByteRange = annotation.sentence(sentenceIdx);
  return asStringView(sentenceAsByteRange);
}

void AnnotatedBlob::addSentence(std::vector<string_view> &wordRanges) {
  addSentence(std::begin(wordRanges), std::end(wordRanges));
};

void AnnotatedBlob::addSentence(std::vector<string_view>::iterator begin,
                                std::vector<string_view>::iterator end) {
  std::vector<ByteRange> sentence;
  for (auto p = begin; p != end; p++) {
    size_t begin_offset = p->data() - &blob[0];
    sentence.push_back((ByteRange){begin_offset, begin_offset + p->size() - 1});
  }
  annotation.addSentence(sentence);
};

ByteRange AnnotatedBlob::wordAsByteRange(size_t sentenceIdx,
                                         size_t wordIdx) const {
  return annotation.word(sentenceIdx, wordIdx);
}

ByteRange AnnotatedBlob::sentenceAsByteRange(size_t sentenceIdx) const {
  return annotation.sentence(sentenceIdx);
}

string_view AnnotatedBlob::asStringView(const ByteRange &byteRange) const {
  const char *data = &blob[byteRange.begin_byte_offset];
  size_t size = byteRange.size();
  return string_view(data, size);
}

} // namespace bergamot
} // namespace marian
