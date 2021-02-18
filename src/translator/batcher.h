#ifndef SRC_BERGAMOT_BATCHER_H_
#define SRC_BERGAMOT_BATCHER_H_

#include "batch.h"
#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "pcqueue.h"
#include "request.h"
#include "request_tracker.h"

#include <mutex>
#include <set>
#include <vector>

namespace marian {
namespace bergamot {
class Batcher {
public:
  explicit Batcher(Ptr<Options> options);

  // RequestSentence incorporates (tentative) notions of priority with each
  // sentence. This method inserts the sentence into the internal data-structure
  // which maintains priority among sentences from multiple concurrent requests.
  void addSentenceWithPriority(RequestSentence &sentence);
  StatusCode addWholeRequest(Ptr<Request> request);
  void produceTo(PCQueue<Batch> &pcqueue);

  void cancel(RequestTracker *tracker);
  void amend(RequestTracker *tracker, int nice);

  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  bool cleaveBatch(Batch &batch);
  bool operator>>(Batch &batch); // alias

private:
  size_t miniBatchWords_;
  std::vector<std::set<RequestSentence>> bucket_;
  size_t batchNumber_{0};
  std::mutex bucketAccess_;
  std::atomic<size_t> maxiBatchWords_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_BATCHER_H_
