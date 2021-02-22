
#include "request_tracker.h"

namespace marian {
namespace bergamot {

RequestTracker::RequestTracker() { timer_.start(); };
void RequestTracker::track(Ptr<Request> request) {
  request_ = request;
  future = request_->get_future();
}

void RequestTracker::logStatusChange(StatusCode before, StatusCode after) {
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
      humanfriendly(before), humanfriendly(after));
  if (after == StatusCode::SUCCESS) {
    LOG(info, "Request({}) completed in {}s wall", request_->Id(),
        timer_.elapsed());
  }
}

void RequestTracker::setStatus(StatusCode code) {
  // logStatusChange(status_, code);
  status_ = code;
}

} // namespace bergamot
} // namespace marian
