#ifndef WASM_COMPATIBLE_SOURCE
#include "threadsafe_batcher.h"

#include <cassert>

namespace marian {
namespace bergamot {

ThreadsafeBatcher::ThreadsafeBatcher(Ptr<Options> options) : backend_(options), enqueued_(0), shutdown_(false) {}

ThreadsafeBatcher::~ThreadsafeBatcher() { shutdown(); }

void ThreadsafeBatcher::addWholeRequest(Ptr<Request> request) {
  std::unique_lock<std::mutex> lock(mutex_);
  assert(!shutdown_);
  backend_.addWholeRequest(request);
  enqueued_ += request->numToBeFreshlyTranslated();
  work_.notify_all();
}

void ThreadsafeBatcher::shutdown() {
  std::unique_lock<std::mutex> lock(mutex_);
  shutdown_ = true;
  work_.notify_all();
}

bool ThreadsafeBatcher::operator>>(Batch &batch) {
  std::unique_lock<std::mutex> lock(mutex_);
  work_.wait(lock, [this]() { return enqueued_ || shutdown_; });
  bool ret = backend_ >> batch;
  assert(ret || shutdown_);
  enqueued_ -= batch.size();
  return ret;
}

}  // namespace bergamot
}  // namespace marian
#endif  // WASM_COMPATIBLE_SOURCE
