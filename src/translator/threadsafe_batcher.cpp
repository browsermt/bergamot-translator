#ifndef WASM_COMPATIBLE_SOURCE
#include "threadsafe_batcher.h"

#include <cassert>

namespace marian {
namespace bergamot {

ThreadsafeAggregateBatchingPool::ThreadsafeAggregateBatchingPool() : enqueued_(0), shutdown_(false) {}

ThreadsafeAggregateBatchingPool::~ThreadsafeAggregateBatchingPool() { shutdown(); }

void ThreadsafeAggregateBatchingPool::addRequest(Ptr<TranslationModel> translationModel, Ptr<Request> request) {
  std::unique_lock<std::mutex> lock(mutex_);
  assert(!shutdown_);
  backend_.addRequest(translationModel, request);
  enqueued_ += request->numSegments();
  work_.notify_all();
}

void ThreadsafeAggregateBatchingPool::shutdown() {
  std::unique_lock<std::mutex> lock(mutex_);
  shutdown_ = true;
  work_.notify_all();
}

bool ThreadsafeAggregateBatchingPool::generateBatch(Ptr<TranslationModel> &translationModel, Batch &batch) {
  std::unique_lock<std::mutex> lock(mutex_);
  work_.wait(lock, [this]() { return enqueued_ || shutdown_; });
  bool ret = backend_.generateBatch(translationModel, batch);
  assert(ret || shutdown_);
  enqueued_ -= batch.size();
  return ret;
}

}  // namespace bergamot
}  // namespace marian
#endif  // WASM_COMPATIBLE_SOURCE
