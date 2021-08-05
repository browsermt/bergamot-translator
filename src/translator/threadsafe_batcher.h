/* Thread-safe wrapper around batcher. */
#ifndef SRC_BERGAMOT_THREADSAFE_BATCHER_H_
#define SRC_BERGAMOT_THREADSAFE_BATCHER_H_

#include "aggregate_batching_pool.h"
#include "batching_pool.h"
#include "common/options.h"
#include "definitions.h"
#include "translation_model.h"

#ifndef WASM_COMPATIBLE_SOURCE
#include <condition_variable>
#include <mutex>

namespace marian {
namespace bergamot {

template <class BatchingPoolType>
class GuardedBatchingPoolAccess {
 public:
  GuardedBatchingPoolAccess(BatchingPoolType &backend);
  ~GuardedBatchingPoolAccess();

  template <class... Args>
  void addRequest(Args &&... args);

  template <class... Args>
  size_t generateBatch(Args &&... args);

  void shutdown();

 private:
  BatchingPoolType &backend_;

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

#endif  // WASM_COMPATIBLE_SOURCE
#endif  // SRC_BERGAMOT_THREADSAFE_BATCHER_H_
