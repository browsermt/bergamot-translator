#include "batcher.h"
#include "batch.h"
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
  size_t bucket_id = sentence.numTokens();
  assert(bucket_id < bucket_.size());
  bucket_[bucket_id].insert(sentence);
}

bool Batcher::operator>>(Batch &batch) { return cleaveBatch(batch); }

bool Batcher::cleaveBatch(Batch &batch) {
  // For now simply iterates on buckets and converts batches greedily.  This
  // has to be enhanced with optimizing over priority. The baseline
  // implementation should at least be as fast as marian's maxi-batch with full
  // corpus size as maxi-batch size.
  batch.clear();
  size_t paddedBatchSize = 0;

  for (size_t length = 0; length < bucket_.size(); length++) {
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
        return true;
      }
    }
  }

  bool isValidBatch = batch.size() > 0;
  return isValidBatch;
}

void Batcher::addWholeRequest(Ptr<Request> request) {
  for (size_t i = 0; i < request->numSegments(); i++) {
    RequestSentence requestSentence(i, request);
    addSentenceWithPriority(requestSentence);
  }
}

void Batcher::produceTo(PCQueue<Batch> &pcqueue) {
  Batch batch;
  while (cleaveBatch(batch)) {
    pcqueue.ProduceSwap(batch);
  }
}

} // namespace bergamot
} // namespace marian
