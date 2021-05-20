#include "batch.h"

#include "request.h"

namespace marian {
namespace bergamot {

void Batch::log() {
  size_t numTokens{0}, maxLength{0};
  for (auto &sentence : sentences_) {
    numTokens += sentence.numTokens();
    maxLength = std::max(maxLength, static_cast<size_t>(sentence.numTokens()));
  }

  LOG(info, "Batch(tokens={}, max-length={}, sentences_={})", numTokens, maxLength, sentences_.size());
}

void Batch::add(const RequestSentence &sentence) { sentences_.push_back(sentence); }

void Batch::completeBatch(const Histories &histories) {
  for (size_t i = 0; i < sentences_.size(); i++) {
    sentences_[i].completeSentence(histories[i]);
  }
}
}  // namespace bergamot
}  // namespace marian
