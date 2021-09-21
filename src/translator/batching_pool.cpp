#include "batching_pool.h"

#include <cassert>

#include "batch.h"
#include "common/logging.h"

namespace marian {
namespace bergamot {

BatchingPool::BatchingPool(Ptr<Options> options) {
  miniBatchWords = options->get<int>("mini-batch-words");
  bucket_.resize(options->get<int>("max-length-break") + 1);
  ABORT_IF(bucket_.size() - 1 > miniBatchWords,
           "Fatal: max-length-break > mini-batch-words  will lead to sentences "
           "longer than what can fit in a batch.");
}

size_t BatchingPool::generateBatch(Batch &batch) {
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
        return batch.size();
      }
    }
  }

  return batch.size();
}

size_t BatchingPool::enqueueRequest(Ptr<Request> request) {
  for (size_t i = 0; i < request->numSegments(); i++) {
    if (!request->isCachePrefilled(i)) {
      RequestSentence sentence(i, request);
      size_t bucket_id = sentence.numTokens();
      assert(bucket_id < bucket_.size());
      bucket_[bucket_id].insert(sentence);
    }
  }

  return request->numSegments();
}

}  // namespace bergamot
}  // namespace marian
