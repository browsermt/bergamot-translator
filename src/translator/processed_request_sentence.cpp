#include "processed_request_sentence.h"

#include <cstring>
#include <iostream>

namespace marian {
namespace bergamot {

namespace {

// Generic write/read from pointers

// write uses ostream, to get bytes/blob use with ostringstream(::binary)
template <class T>
void write(std::ostream &out, const T *data, size_t num = 1) {
  const char *cdata = reinterpret_cast<const char *>(data);
  out.write(cdata, num * sizeof(T));
}

// L4 stores the data as blobs. These use memcpy to construct parts from the blob given the start and size. It is the
// responsibility of the caller to prepare the container with the correct contiguous size so memcpy works correctly.
template <class T>
const char *copyInAndAdvance(const char *src, T *dest, size_t num = 1) {
  const void *vsrc = reinterpret_cast<const void *>(src);
  void *vdest = reinterpret_cast<void *>(dest);
  std::memcpy(vdest, vsrc, num * sizeof(T));
  return reinterpret_cast<const char *>(src + num * sizeof(T));
}

// Specializations to read and write vectors. The format stored is [size, v_0, v_1 ... v_size]
template <class T>
void writeVector(std::ostream &out, std::vector<T> v) {
  size_t size = v.size();
  write<size_t>(out, &size);
  write<T>(out, v.data(), v.size());
}

template <class T>
const char *copyInVectorAndAdvance(const char *src, std::vector<T> &v) {
  // Read in size of the vector
  size_t sizePrefix;
  src = copyInAndAdvance<size_t>(src, &sizePrefix);

  // Ensure contiguous memory location exists for memcpy inside copyInAndAdvance
  v.resize(sizePrefix);

  // Read in the vector
  src = copyInAndAdvance<T>(src, v.data(), sizePrefix);
  return src;
}

}  // namespace

ProcessedRequestSentence::ProcessedRequestSentence() {}

/// Construct from History
ProcessedRequestSentence::ProcessedRequestSentence(const History &history) {
  // Convert marian's lazy shallow-history, consolidating just the information we want.
  IPtr<Hypothesis> hypothesis;
  Result result = history.top();
  std::tie(words_, hypothesis, sentenceScore_) = result;
  softAlignment_ = hypothesis->tracebackAlignment();
  wordScores_ = hypothesis->tracebackWordScores();
}

std::string ProcessedRequestSentence::toBytes() const {
  // Note: write in order of member definitions in class.
  std::ostringstream out(std::ostringstream::binary);
  writeVector<marian::Word>(out, words_);

  size_t softAlignmentSize = softAlignment_.size();
  write<size_t>(out, &softAlignmentSize);
  for (auto &alignment : softAlignment_) {
    writeVector<float>(out, alignment);
  }

  write<float>(out, &sentenceScore_);
  writeVector<float>(out, wordScores_);
  return out.str();
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytes(char const *data, size_t size) {
  ProcessedRequestSentence sentence;
  char const *p = data;

  p = copyInVectorAndAdvance<marian::Word>(p, sentence.words_);

  size_t softAlignmentSize{0};
  p = copyInAndAdvance<size_t>(p, &softAlignmentSize);
  sentence.softAlignment_.resize(softAlignmentSize);

  for (size_t i = 0; i < softAlignmentSize; i++) {
    p = copyInVectorAndAdvance<float>(p, sentence.softAlignment_[i]);
  }

  p = copyInAndAdvance<float>(p, &sentence.sentenceScore_);
  p = copyInVectorAndAdvance<float>(p, sentence.wordScores_);
  return sentence;
}
}  // namespace bergamot
}  // namespace marian
