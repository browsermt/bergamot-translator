/* Thread-safe wrapper around batcher. */
#ifndef SRC_BERGAMOT_THREADSAFE_BATCHER_H_
#define SRC_BERGAMOT_THREADSAFE_BATCHER_H_

#include <condition_variable>
#include <mutex>

#include "aggregate_batching_pool.h"
#include "batching_pool.h"
#include "common/options.h"
#include "definitions.h"
#include "translation_model.h"

namespace marian {
namespace bergamot {

/// The following mechanism operates in a multithreaded async-workflow guarding access to the pushes to the structure
/// keeping sentences bucketed by length and sorted by priority.
///
/// This is a wrap of a producer-consumer queue implemented as a monitor, where there is a mutex guarding the
/// underlying data structure and (worker/consumer) threads waiting on a condition variable and the queuing thread
/// producing and notifying waiting threads (consumers) through the same condition variable.
///
/// Originally written by for a single model (where items are produce: Request, consume: Batch), converted to
/// also work for multiple models where items are produce: (TranslationModel, Request), consume: (TranlsationModel,
/// Batch). This is accomplished by template parameter packs.
///
/// Requires BatchingPoolType to implement:
///
/// * size_t addRequest(...)
/// * size_t generateBatch(...)

template <class BatchingPoolType>
class GuardedBatchingPoolAccess {
 public:
  template <class... Args>
  GuardedBatchingPoolAccess(Args &&... args);
  ~GuardedBatchingPoolAccess();

  template <class... Args>
  void addRequest(Args &&... args);

  template <class... Args>
  size_t generateBatch(Args &&... args);

  void shutdown();

 private:
  BatchingPoolType backend_;

  // Number of sentences in backend_;
  size_t enqueued_;

  // Are we shutting down?
  bool shutdown_;

  // Lock on this object.
  std::mutex mutex_;

  // Signaled when there are sentences to translate.
  std::condition_variable work_;
};

}  // namespace bergamot
}  // namespace marian

#define SRC_BERGAMOT_THREADSAFE_BATCHER_IMPL
#include "threadsafe_batcher.cpp"
#undef SRC_BERGAMOT_THREADSAFE_BATCHER_IMPL

#endif  // SRC_BERGAMOT_THREADSAFE_BATCHER_H_
