#include "processed_request_sentence.h"

#include <cstring>

namespace marian {
namespace bergamot {

ProcessedRequestSentence::ProcessedRequestSentence() {}

ProcessedRequestSentence::ProcessedRequestSentence(const string_view &bytesView)
    : storage_(bytesView.data(), bytesView.size()) {
  words_ = storage_.readRange<Words::value_type>();
  softAlignmentSizePtr_ = storage_.readVar<size_t>();
  for (size_t i = 0; i < *(softAlignmentSizePtr_); i++) {
    auto unitAlignment = storage_.readRange<DistSourceGivenTarget::value_type>();
    softAlignment_.push_back(std::move(unitAlignment));
  }

  wordScores_ = storage_.readRange<WordScores::value_type>();
  sentenceScorePtr_ = storage_.readVar<float>();
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
  words_ = storage_.writeRange<Words::value_type>(words);
  softAlignmentSizePtr_ = storage_.writeVar<size_t>(softAlignment.size());
  for (size_t i = 0; i < softAlignment.size(); i++) {
    auto unitAlignment = storage_.writeRange<DistSourceGivenTarget::value_type>(softAlignment[i]);
    softAlignment_.push_back(std::move(unitAlignment));
  }

  wordScores_ = storage_.writeRange<WordScores::value_type>(wordScores);
  sentenceScorePtr_ = storage_.writeVar<float>(sentenceScore);
}

string_view ProcessedRequestSentence::toBytesView() const {
  return string_view(reinterpret_cast<const char *>(storage_.data()), storage_.size());
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytesView(const string_view &bytesView) {
  return ProcessedRequestSentence(bytesView);
}

}  // namespace bergamot
}  // namespace marian
