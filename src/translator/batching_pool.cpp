#include "batching_pool.h"

#include <cassert>

#include "batch.h"
#include "common/logging.h"

namespace marian {
namespace bergamot {

BatchingPool::BatchingPool(Ptr<Options> options)
    : miniBatchWords_(options->get<int>("mini-batch-words")), maxActiveBucketLength_(0) {
  size_t maxLengthBreak = options->get<int>("max-length-break");
  float maxLengthFactor = options->get<float>("max-length-factor", 3.0);

  // For the time being, we add some slack, which only BatchingPool is aware of. Since the TextProcessor still wraps at
  // first request in, most of the Batches generated will be under max-length break.
  //
  // In the unlikely event of a few sentences overflowing, this allows the exceeding words to be put in the slack area.
  // Very few batches are expected to be generated at a higher length.
  size_t pivotSlack = maxLengthBreak * maxLengthFactor - maxLengthBreak;
  bucket_.resize(maxLengthBreak + pivotSlack + 1);

  ABORT_IF(bucket_.size() - 1 > miniBatchWords_,
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

  for (size_t length = 0; length <= maxActiveBucketLength_; length++) {
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
        return batch.size();
      }
    }
  }

  return batch.size();
}

size_t BatchingPool::enqueueRequest(Ptr<Request> request) {
  size_t toBeFreshlyTranslated = 0;
  for (size_t i = 0; i < request->numSegments(); i++) {
    if (!request->cacheHitPrefilled(i)) {
      RequestSentence sentence(i, request);
      size_t bucket_id = sentence.numTokens();

      // Due to a workaround for pivoting, unless we can discipline the
      // vocabulary to get stronger static requirements, it is difficult to
      // rework the rest of the components. Instead, we allow dynamic growth
      // here. We let std::vector take care of the dynamic growth.
      // https://en.cppreference.com/w/cpp/container/vector/resize#Complexity
      if (bucket_id >= bucket_.size()) {
        bucket_.resize(bucket_id + 1);
      }

      bucket_[bucket_id].insert(sentence);
      maxActiveBucketLength_ = std::max<size_t>(bucket_id, maxActiveBucketLength_);

      toBeFreshlyTranslated += 1;
    }
  }

  return toBeFreshlyTranslated;
}

void BatchingPool::clear() {
  for (size_t length = 0; length < bucket_.size(); length++) {
    bucket_[length].clear();
  }
}

}  // namespace bergamot
}  // namespace marian
