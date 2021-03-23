#ifndef SRC_BERGAMOT_BATCHER_H_
#define SRC_BERGAMOT_BATCHER_H_

#include "batch.h"
#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "request.h"
#include "request_tracker.h"

#ifndef WASM
#include "pcqueue.h"
#include <mutex>
#endif

#include <set>
#include <vector>

namespace marian {
namespace bergamot {

class Batcher {
public:
  /// Expects the following options to be set:  `mini-batch-words`,
  /// `max-length-break`.
  // @TODO(jerinphilip): Probably make this named parameters.
  explicit Batcher(Ptr<Options> options);

  /// RequestSentence incorporates notions of priority with each
  /// sentence. This method inserts the sentence into the internal
  /// data-structure which maintains priority among sentences from multiple
  /// concurrent requests.
  void addSentenceWithPriority(RequestSentence &sentence);

  /// Adds a whole Request. Internally calls addSentenceWithPriority.
  void addWholeRequest(Ptr<Request> request);

  void cancel(RequestTracker *tracker);
  void amend(RequestTracker *tracker, int nice);

  /// alias for cleaveBatch, for use outside this class.
  /// \code{.cpp}
  ///   while (batcher >> batch) {
  ///      // do something;
  ///    }
  /// \endcode
  bool operator>>(Batch &batch);

private:
  // Cleaves of batches with sentences compiled from multiple requests
  // optimizing for both padding and priority. Returns false when no more
  // batches can be created due to lack of sentences.
  bool cleaveBatch(Batch &batch);

  size_t miniBatchWords_;

  // Internal data structure to generate batches optimized by priority and
  // packing efficiency.
  std::vector<std::set<RequestSentence>> bucket_;

  size_t batchNumber_{0};

  // Controls access to bucket_ among concurrent queuing requests.
  std::mutex bucketAccess_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_BATCHER_H_
