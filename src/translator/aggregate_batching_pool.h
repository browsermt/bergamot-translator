#ifndef SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_
#define SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_

#include <memory>
#include <queue>

#include "data/types.h"
#include "translation_model.h"

namespace marian {
namespace bergamot {

class AggregateBatchingPool {
 public:
  void addRequest(Ptr<TranslationModel> model, Ptr<Request> request);
  bool generateBatch(Ptr<TranslationModel>& model, Batch& batch);

 private:
  std::queue<std::weak_ptr<TranslationModel>> aggregateQueue_;
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_AGGREGATE_BATCHING_POOL_H_
