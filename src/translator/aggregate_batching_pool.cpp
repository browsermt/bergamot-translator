
#include "aggregate_batching_pool.h"

namespace marian {
namespace bergamot {

AggregateBatchingPool::AggregateBatchingPool() {
  // TODO(@jerinphilip): Set aggregate limits
}

size_t AggregateBatchingPool::enqueueRequest(Ptr<TranslationModel> model, Ptr<Request> request) {
  aggregateQueue_.insert(model);
  return model->enqueueRequest(request);
}

size_t AggregateBatchingPool::generateBatch(Ptr<TranslationModel>& model, Batch& batch) {
  while (!aggregateQueue_.empty()) {
    auto candidateItr = aggregateQueue_.begin();
    Ptr<TranslationModel> candidate = *candidateItr;
    size_t numSentences = candidate->generateBatch(batch);
    if (numSentences > 0) {
      model = candidate;
      return numSentences;
    } else {
      // Try the next model's batching pool.
      aggregateQueue_.erase(candidateItr);
    }
  }
  return /*numSentences=*/0;
}

}  // namespace bergamot
}  // namespace marian
