#include <data/types.h>
#include <future>

namespace marian {
namespace bergamot {

enum StatusCode {
  UNSET,               // No object has operated yet.
  CANCELLED_BY_USER,   // Denotes if the Request was cancelled by user.
  REJECTED_MEMORY,     // Rejected by batcher on memory constraints.
  QUEUED,              // Successfully Queued
  TRANSLATION_SUCCESS, // Successfully Translated
};

class RequestTracker {
  // A fancier std::promise <-> std::future, with abilities to cancel and amend
  // priorities.
public:
  RequestTracker() {}

  void track(Ptr<Request> request) {
    request_ = request;
    future = request_->get_future();
  }

  std::atomic<StatusCode> status{StatusCode::UNSET};
  std::future<Response> future;

  void cancel(); // Cancel. Only sets the future_ object and status_ codes
                 // accordingly. It's the responsibility of whoever owns to
                 // forward cancel calls to Batcher/Translator.
  const Ptr<Request> &request() const { return request_; }

private:
  Ptr<Request> request_{nullptr};
  bool initialized_{false};
};

} // namespace bergamot
} // namespace marian
