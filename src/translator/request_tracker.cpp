
#include "request_tracker.h"

namespace marian {
namespace bergamot {

RequestTracker::RequestTracker() { timer_.start(); };
void RequestTracker::track(Ptr<Request> request) {
  request_ = request;
  future = request_->get_future();
}

void RequestTracker::setStatus(StatusCode code) {
  auto humanfriendly = [](StatusCode scode) {
    std::string hfcode;
    switch (scode) {
    case StatusCode::UNSET:
      hfcode = "UNSET";
      break;
    case StatusCode::CANCELLED_BY_USER:
      hfcode = "CANCELLED_BY_USER";
      break;
    case StatusCode::REJECTED_MEMORY:
      hfcode = "REJECTED_MEMORY";
      break;
    case StatusCode::QUEUED:
      hfcode = "QUEUED";
      break;
    case StatusCode::SUCCESS:
      hfcode = "SUCCESS";
      break;
    default:
      hfcode = "UNKNOWN";
      break;
    }
    return hfcode;
  };

  LOG(info, "Request({}) status change: {} -> {}", request_->Id(),
      humanfriendly(status_), humanfriendly(code));
  status_ = code;
}

} // namespace bergamot
} // namespace marian
