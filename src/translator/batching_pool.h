#ifndef SRC_BERGAMOT_BATCHING_POOL_H_
#define SRC_BERGAMOT_BATCHING_POOL_H_

#include <set>
#include <vector>

#include "batch.h"
#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "request.h"

namespace marian {
namespace bergamot {

class BatchingPool {
 public:
  explicit BatchingPool(Ptr<Options> options);

  // RequestSentence incorporates (tentative) notions of priority with each
  // sentence. This method inserts the sentence into the internal data-structure
  // which maintains priority among sentences from multiple concurrent requests.
  size_t enqueueRequest(Ptr<Request> request);

  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  size_t generateBatch(Batch &batch);

 private:
  size_t miniBatchWords;
  std::vector<std::set<RequestSentence>> bucket_;
  size_t batchNumber_{0};
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_BATCHING_POOL_H_
