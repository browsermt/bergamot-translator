
#ifndef SRC_BERGAMOT_THREADSAFE_BATCHING_POOL_IMPL
#error "This is an impl file and must not be included directly!"
#endif

#include <cassert>

namespace marian {
namespace bergamot {

template <class BatchingPoolType>
template <class... Args>
ThreadsafeBatchingPool<BatchingPoolType>::ThreadsafeBatchingPool(Args &&...args)
    : backend_(std::forward<Args>(args)...), enqueued_(0), shutdown_(false) {}

template <class BatchingPoolType>
ThreadsafeBatchingPool<BatchingPoolType>::~ThreadsafeBatchingPool() {
  shutdown();
}

template <class BatchingPoolType>
template <class... Args>
void ThreadsafeBatchingPool<BatchingPoolType>::enqueueRequest(Args &&...args) {
  std::unique_lock<std::mutex> lock(mutex_);
  assert(!shutdown_);
  enqueued_ += backend_.enqueueRequest(std::forward<Args>(args)...);
  work_.notify_all();
}

template <class BatchingPoolType>
void ThreadsafeBatchingPool<BatchingPoolType>::clear() {
  std::unique_lock<std::mutex> lock(mutex_);
  backend_.clear();
  enqueued_ = 0;
}

template <class BatchingPoolType>
void ThreadsafeBatchingPool<BatchingPoolType>::shutdown() {
  std::unique_lock<std::mutex> lock(mutex_);
  shutdown_ = true;
  work_.notify_all();
}

template <class BatchingPoolType>
template <class... Args>
size_t ThreadsafeBatchingPool<BatchingPoolType>::generateBatch(Args &&...args) {
  std::unique_lock<std::mutex> lock(mutex_);
  work_.wait(lock, [this]() { return enqueued_ || shutdown_; });
  size_t sentencesInBatch = backend_.generateBatch(std::forward<Args>(args)...);
  assert(sentencesInBatch > 0 || shutdown_);
  enqueued_ -= sentencesInBatch;
  return sentencesInBatch;
}

}  // namespace bergamot
}  // namespace marian
