#include "batcher.h"
#include "batch.h"
#include "common/logging.h"
#include <cassert>
#include <stdint.h>

namespace marian {
namespace bergamot {

Batcher::Batcher(Ptr<Options> options) {
  rejectOnFull_ = options->get<bool>("reject-on-full");
  miniBatchWords_ = options->get<int>("mini-batch-words");
  // TODO(jerinphilip):
  // maxiBatchWords_ = options->get<int>("maxi-batch-words");
  maxiBatchWords_ = SIZE_MAX;
  bucket_.resize(options->get<int>("max-length-break") + 1);
  ABORT_IF(bucket_.size() - 1 > miniBatchWords_,
           "Fatal: max-length-break > mini-batch-words  will lead to sentences "
           "longer than what can fit in a batch.");
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
  std::lock_guard<std::mutex> lock(bucketAccess_);
  batch.clear();
  size_t paddedBatchSize = 0;

  for (size_t length = 0; length < bucket_.size(); length++) {
    auto p = bucket_[length].begin();
    while (p != bucket_[length].end()) {
      paddedBatchSize = (batch.size() + 1) * length;
      if (paddedBatchSize <= miniBatchWords_) {
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

StatusCode Batcher::addWholeRequest(Ptr<Request> request) {

  size_t requestWords = request->numWords();
  if (rejectOnFull_ && maxiBatchWords_ < requestWords) {
    return StatusCode::REJECTED_MEMORY;
  } else {
    // TODO(jerinphilip): Do we really need this? Preproc is already done. All
    // the queue structure does is point to Request objects. PCQueue block on
    // limits happens at produceTo.
    //
    // blockTillSpaceAvailable(requestWords);
    / std::lock_guard<std::mutex> lock(bucketAccess_);
    for (size_t i = 0; i < request->numSegments(); i++) {
      RequestSentence requestSentence(i, request);
      addSentenceWithPriority(requestSentence);
    }

    return StatusCode::QUEUED;
  }
}

void Batcher::produceTo(PCQueue<Batch> &pcqueue) {
  Batch batch;
  while (cleaveBatch(batch)) {
    pcqueue.ProduceSwap(batch);
  }
}

void Batcher::cancel(RequestTracker *tracker) {
  tracker->setStatus(StatusCode::CANCELLED_BY_USER);
  // TODO(jerinphilip): Cancel code
}

void Batcher::amend(RequestTracker *tracker, int nice) {
  // TODO(jerinphilip): Amend code
}

} // namespace bergamot
} // namespace marian
