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

class RequestTracker {
  // A fancier std::promise <-> std::future, with abilities to cancel and amend
  // priorities.
public:
  std::future<Response> future;

  // Empty construction, and set later to track a request.
  RequestTracker();
  void track(Ptr<Request> request);

  // Currently changeable, TODO(jerinphilip) disallow except through friends -
  // Service, Request?
  void setStatus(StatusCode code);

  // Returns status.
  const StatusCode status() const { return status_; }

  // Returns access to the underlying pointer.
  const Ptr<Request> &request() const { return request_; }

private:
  Ptr<Request> request_{nullptr};
  StatusCode status_{StatusCode::UNSET};

  // Convenience function to log StatusChanges and times.
  void logStatusChange(StatusCode before, StatusCode after);

  // Temporary book-keeping
  timer::Timer timer_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_REQUEST_TRACKER_H
