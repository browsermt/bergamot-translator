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

  RequestTracker();
  void track(Ptr<Request> request);
  void setStatus(StatusCode code);

  const StatusCode status() const { return status_; }
  const Ptr<Request> &request() const { return request_; }

private:
  Ptr<Request> request_{nullptr};
  StatusCode status_{StatusCode::UNSET};

  // Temporary book-keeping
  timer::Timer timer_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_REQUEST_TRACKER_H
