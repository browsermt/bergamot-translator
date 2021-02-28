#ifndef SRC_BERGAMOT_REQUEST_TRACKER_H
#define SRC_BERGAMOT_REQUEST_TRACKER_H

#include "definitions.h"
#include "request.h"
#include "response.h"
#include <common/logging.h>
#include <common/timer.h>
#include <data/types.h>
#include <future>

namespace marian {
namespace bergamot {

class Request;

/// A tracker/ticket provided to someone who calls Service#translate(...).
/// RequestTracker provides controlled access to the underlying Request to
/// enable capabilities of cancellation and amending priorities.
class RequestTracker {
public:
  std::future<Response> future;

  /// Empty construction, and set later to track a request.
  RequestTracker();

  /// Sets tracking on a request.
  void track(Ptr<Request> request);

  /// Sets status: Used when the status of a request changes. For example, when
  /// translation completes, status code is set to SUCCESS. See
  /// StatusCode.
  void setStatus(StatusCode code);
  // Currently changeable, TODO(jerinphilip) disallow except through friends -
  // Service, Request?

  const StatusCode status() const { return status_; }
  const Ptr<Request> &request() const { return request_; }

  /// Sets future captured by the tracker.
  void set_future(std::future<Response> &&responseFuture);
  void wait() { future.wait(); }

private:
  Ptr<Request> request_{nullptr};
  StatusCode status_{StatusCode::UNSET};

  /// Convenience function to log StatusChanges and times.
  void logStatusChange(StatusCode before, StatusCode after);

  // Temporary book-keeping
  timer::Timer timer_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_REQUEST_TRACKER_H
