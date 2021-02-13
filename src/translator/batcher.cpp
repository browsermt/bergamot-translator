#include "batcher.h"
#include "common/logging.h"
#include <cassert>

namespace marian {
namespace bergamot {

Batcher::Batcher(Ptr<Options> options) {
  miniBatchWords = options->get<int>("max-input-tokens");
  bucket_.resize(options->get<int>("max-input-sentence-tokens") + 1);
  ABORT_IF(
      miniBatchWords < bucket_.size() - 1,
      "max-input-tokens cannot be less than than max-input-sentence-tokens, "
      "batcher fail");
}

void Batcher::addSentenceWithPriority(RequestSentence &sentence) {
  int bucket_id = sentence.numTokens();
  assert(bucket_id < bucket_.size());
  bucket_[bucket_id].insert(sentence);
}

bool Batcher::operator>>(Batch &batch) { return cleaveBatch(batch); }

bool Batcher::cleaveBatch(Batch &batch) {
  // For now simply iterates on buckets and converts batches greedily.  This
  // has to be enhanced with optimizing over priority. The baseline
  // implementation should at least be as fast as marian's maxi-batch with full
  // corpus size as maxi-batch size.
  batch.reset();
  int paddedBatchSize = 0;

  for (int length = 0; length < bucket_.size(); length++) {
    auto p = bucket_[length].begin();
    while (p != bucket_[length].end()) {
      paddedBatchSize = (batch.size() + 1) * length;
      if (paddedBatchSize <= miniBatchWords) {
        auto q = p++;
        batch.add(*q);
        bucket_[length].erase(q);
      } else {
        // Check if elements exist
        assert(batch.size() > 0);
        batch.setId(++batchNumber_);
        return true;
      }
    }
  }

  if (batch.size()) {
    batch.setId(++batchNumber_);
    return true;
  } else {
    return false;
  }
}

void Batcher::addWholeRequest(Ptr<Request> request) {
  for (int i = 0; i < request->numSegments(); i++) {
    RequestSentence requestSentence(i, request);
    addSentenceWithPriority(requestSentence);
  }
}

void Batcher::enqueue(PCQueue<Batch> &pcqueue) {
  Batch batch;
  while (cleaveBatch(batch)) {
    pcqueue.ProduceSwap(batch);
  }
}

} // namespace bergamot
} // namespace marian
