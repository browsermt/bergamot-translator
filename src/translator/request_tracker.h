#ifndef SRC_BERGAMOT_REQUEST_TRACKER_H
#define SRC_BERGAMOT_REQUEST_TRACKER_H

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
  std::future<Response> future;

  RequestTracker(){};
  void track(Ptr<Request> request) {
    request_ = request;
    future = request_->get_future();
  }

  const StatusCode status() const { return status_; }

  void setStatus(StatusCode code) { status_ = code; }
  const Ptr<Request> &request() const { return request_; }

private:
  Ptr<Request> request_{nullptr};
  std::atomic<StatusCode> status_{StatusCode::UNSET};
};

} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_REQUEST_TRACKER_H
