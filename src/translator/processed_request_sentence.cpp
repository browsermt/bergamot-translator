#include "processed_request_sentence.h"

#include <cstring>

namespace marian {
namespace bergamot {

namespace {

// Read and seek pointer forward.
template <class T>
void readVarAndAdvance(T *&var, void *&ptr) {
  var = reinterpret_cast<T *>(ptr);
  ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ptr) + sizeof(T));
}

// For writing and seeking forward
template <class T>
void writeVarAndAdvance(const T &value, T *&var, void *&ptr) {
  // Variable is marked to point to pointer, then the incoming value written.
  var = reinterpret_cast<T *>(ptr);
  *var = value;

  // Advance pointer after write.
  ptr = reinterpret_cast<void *>(reinterpret_cast<char *>(ptr) + sizeof(T));
}

}  // namespace

ProcessedRequestSentence::ProcessedRequestSentence() {}

ProcessedRequestSentence::ProcessedRequestSentence(const char *data, size_t size) : storage_(data, size) {
  void *readMarker = storage_.data();
  words_ = Words(readMarker);
  readVarAndAdvance<size_t>(softAlignmentSizePtr_, readMarker);
  for (size_t i = 0; i < *(softAlignmentSizePtr_); i++) {
    softAlignment_.emplace_back(readMarker);
  }

  wordScores_ = WordScores(readMarker);
  readVarAndAdvance<float>(sentenceScorePtr_, readMarker);
}

ProcessedRequestSentence::ProcessedRequestSentence(const History &history) {
  Result result = history.top();
  auto [words, hypothesis, sentenceScore] = result;

  auto softAlignment = hypothesis->tracebackAlignment();
  auto wordScores = hypothesis->tracebackWordScores();

  // Compute size required, and allocate.
  size_t alignmentStorageSize = 0;
  for (size_t i = 0; i < softAlignment.size(); i++) {
    alignmentStorageSize += DistSourceGivenTarget::storageSize(softAlignment[i]);
  }

  size_t requiredSize = (                    // Aggregate size computation
      Words::storageSize(words)              //
      + sizeof(size_t)                       // softAlignment.size()
      + alignmentStorageSize                 //
      + WordScores::storageSize(wordScores)  //
      + sizeof(float)                        // sentenceScore
  );

  storage_.delayedAllocate(requiredSize);

  // Write onto the allocated space.
  void *writeMarker = storage_.data();
  words_ = Words(words, writeMarker);
  writeVarAndAdvance<size_t>(softAlignment.size(), softAlignmentSizePtr_, writeMarker);
  for (size_t i = 0; i < softAlignment.size(); i++) {
    softAlignment_.emplace_back(softAlignment[i], writeMarker);
  }

  wordScores_ = WordScores(wordScores, writeMarker);
  writeVarAndAdvance<float>(sentenceScore, sentenceScorePtr_, writeMarker);
}

string_view ProcessedRequestSentence::byteStorage() const {
  return string_view(reinterpret_cast<const char *>(storage_.data()), storage_.size());
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytes(const char *data, size_t size) {
  return ProcessedRequestSentence(data, size);
}

}  // namespace bergamot
}  // namespace marian
