
#ifndef SRC_BERGAMOT_THREADSAFE_BATCHER_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <cassert>

namespace marian {
namespace bergamot {

template <class BatchingPoolType>
GuardedBatchingPoolAccess<BatchingPoolType>::GuardedBatchingPoolAccess(BatchingPoolType &backend)
    : backend_(backend), enqueued_(0), shutdown_(false) {}

template <class BatchingPoolType>
GuardedBatchingPoolAccess<BatchingPoolType>::~GuardedBatchingPoolAccess() {
  shutdown();
}

template <class BatchingPoolType>
template <class... Args>
void GuardedBatchingPoolAccess<BatchingPoolType>::addRequest(Args &&... args) {
  std::unique_lock<std::mutex> lock(mutex_);
  assert(!shutdown_);
  enqueued_ += backend_.addRequest(std::forward<Args>(args)...);
  work_.notify_all();
}

template <class BatchingPoolType>
void GuardedBatchingPoolAccess<BatchingPoolType>::shutdown() {
  std::unique_lock<std::mutex> lock(mutex_);
  shutdown_ = true;
  work_.notify_all();
}

template <class BatchingPoolType>
template <class... Args>
size_t GuardedBatchingPoolAccess<BatchingPoolType>::generateBatch(Args &&... args) {
  std::unique_lock<std::mutex> lock(mutex_);
  work_.wait(lock, [this]() { return enqueued_ || shutdown_; });
  size_t sentencesInBatch = backend_.generateBatch(std::forward<Args>(args)...);
  assert(sentencesInBatch > 0 || shutdown_);
  enqueued_ -= sentencesInBatch;
  return sentencesInBatch;
}

}  // namespace bergamot
}  // namespace marian
