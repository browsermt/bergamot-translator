#include "batcher.h"
#include "common/logging.h"
#include "sanelogging.h"
#include <cassert>

namespace marian {
namespace bergamot {

Batcher::Batcher(Ptr<Options> options) {
  max_input_tokens_ = options->get<int>("max-input-tokens");
  bucket_.resize(options->get<int>("max-input-sentence-tokens") + 1);
  ABORT_IF(max_input_tokens_ >= bucket_.size(),
           "max-input-sentence-tokens cannot be greater than max-input-tokens, "
           "batcher fail");
}

void Batcher::addSentenceWithPriority(RequestSentence &sentence) {
  int bucket_id = sentence.numTokens();
  assert(bucket_id < bucket_.size());
  bucket_[bucket_id].insert(sentence);
}

void Batcher::cleaveBatch(RequestSentences &sentences) {
  // For now simply iterates on buckets and converts batches greedily.  This
  // has to be enhanced with optimizing over priority. The baseline
  // implementation should at least be as fast as marian's maxi-batch with full
  // corpus size as maxi-batch size.

  int segments_added = 0;
  int current_input_tokens = 0;
  int padded_batch_size = 0;
  int prev_padded_batch_size;

  for (int i = 0; i < bucket_.size(); i++) {
    auto p = bucket_[i].begin();
    while (p != bucket_[i].end()) {
      padded_batch_size = (segments_added + 1) * i;
      if (padded_batch_size <= max_input_tokens_) {
        auto q = p;
        ++p;
        current_input_tokens += i;
        sentences.push_back(*q);
        ++segments_added;
        bucket_[i].erase(q);
        prev_padded_batch_size = padded_batch_size;
      } else {
        return;
      }
    }
  }
}

} // namespace bergamot
} // namespace marian
