/* Thread-safe wrapper around batcher. */
#ifndef SRC_BERGAMOT_THREADSAFE_BATCHER_H_
#define SRC_BERGAMOT_THREADSAFE_BATCHER_H_

#include "batch_translator.h"
#include "batcher.h"
#include "common/options.h"
#include "definitions.h"

#ifndef WASM_COMPATIBLE_SOURCE
#include <condition_variable>
#include <mutex>
#endif

namespace marian {
namespace bergamot {

#ifdef WASM_COMPATIBLE_SOURCE
// No threads, no locks.
typedef AggregateBatchingPool ThreadsafeBatcher;
#else

class ThreadsafeBatcher {
 public:
  explicit ThreadsafeBatcher(Ptr<Options> options);

  ~ThreadsafeBatcher();

  // Add sentences to be translated by calling these (see Batcher).  When
  // done, call shutdown.
  void addRequest(Ptr<TranslationModel> translationModel, Ptr<Request> request);
  void shutdown();

  // Get a batch out of the batcher.  Return false to shutdown worker.
  bool generateBatch(Ptr<TranslationModel> &translationModel, Batch &batch);

 private:
  AggregateBatchingPool backend_;

  // Number of sentences in backend_;
  size_t enqueued_;

  // Are we shutting down?
  bool shutdown_;

  // Lock on this object.
  std::mutex mutex_;

  // Signaled when there are sentences to translate.
  std::condition_variable work_;
};

#endif

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_THREADSAFE_BATCHER_H_
