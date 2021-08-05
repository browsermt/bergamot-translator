
#include "aggregate_batching_pool.h"

namespace marian {
namespace bergamot {

void AggregateBatchingPool::addRequest(Ptr<TranslationModel> model, Ptr<Request> request) {
  model->addRequest(request);
  aggregateQueue_.push(model);
}
bool AggregateBatchingPool::generateBatch(Ptr<TranslationModel>& model, Batch& batch) {
  while (model == nullptr && !aggregateQueue_.empty()) {
    std::weak_ptr<TranslationModel> weakModel = aggregateQueue_.front();
    model = weakModel.lock();
    if (model) {
      bool retCode = model->generateBatch(batch);
      if (retCode) {
        // We found a batch.
        return true;
      } else {
        // Try the next model's batching pool.
        aggregateQueue_.pop();
      }
    }
  }
  return false;
}

}  // namespace bergamot
}  // namespace marian
