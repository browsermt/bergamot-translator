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

  // Adds a whole Request. Returns statuscode which can either be
  //  a) QUEUED if successful or
  //  b) REJECTED_MEMORY if the instances Batcher/Translator are
  //     operating at full capacity specified by maxiBatchWords .
  StatusCode addWholeRequest(Ptr<Request> request);

  // Launches an infinite loop writing to a producer consumer queue. Only for
  // use asynchonous calls in multithreaded settings.
  void produceTo(PCQueue<Batch> &pcqueue);

  void cancel(RequestTracker *tracker);
  void amend(RequestTracker *tracker, int nice);

  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  bool cleaveBatch(Batch &batch);
  bool operator>>(Batch &batch); // alias

private:
  // miniBatchWords_ specify the size of the batches. This is limited by
  // BatchTranslator and hardware.
  size_t miniBatchWords_;

  // Internal data structure to generate batches optimized by priority and
  // packing efficiency.
  std::vector<std::set<RequestSentence>> bucket_;
  size_t batchNumber_{0};

  // Sets Batcher's operations to reject on full.
  bool rejectOnFull_{false};

  // void blockTillSpaceAvailable(size_t requiredSpace);

  // Controls access to bucket_ among concurrent queeuing requests.
  std::mutex bucketAccess_;

  // maxiBatchWords_ specify how much of a buffer Batcher should keep. Without
  // this construct, Batcher can end up holding infinite memory while waiting to
  // write to pcqueue, which is undesirable.
  std::atomic<int> maxiBatchWords_;
  std::mutex maxiBatchWordsAccess_; // Mutex
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_BATCHER_H_
