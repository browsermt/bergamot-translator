#include "request.h"
#include "definitions.h"
#include "response.h"
#include "sentence_ranges.h"

#include "common/logging.h"

#include <string>

namespace marian {
namespace bergamot {

// -----------------------------------------------------------------
Request::Request(size_t Id, Segments &&segments,
                 ResponseBuilder &&responseBuilder)
    : Id_(Id), segments_(std::move(segments)),
      responseBuilder_(std::move(responseBuilder))

{

  counter_ = segments_.size();
  histories_.resize(segments_.size(), nullptr);

  // If there are no segments_, we are never able to trigger the responseBuilder
  // calls from a different thread. However, in this case we want an empty valid
  // response.
  if (segments_.size() == 0) {
    responseBuilder_(std::move(histories_));
  }
}

size_t Request::numSegments() const { return segments_.size(); }

size_t Request::segmentTokens(size_t index) const {
  return (segments_[index].size());
}

Segment Request::getSegment(size_t index) const { return segments_[index]; }

void Request::processHistory(size_t index, Ptr<History> history) {
  // Concurrently called by multiple workers as a history from translation is
  // ready. The container storing histories is set with the value obtained.
  histories_[index] = history;

  // In case this is last request in, completeRequest is called, which sets the
  // value of the promise.
  if (--counter_ == 0) {
    responseBuilder_(std::move(histories_));
  }
}

bool Request::operator<(const Request &b) const {
  // Among Requests, only sequence id is used for obtaining priority.
  return Id_ < b.Id_;
}

// ------------------------------------------------------------------

RequestSentence::RequestSentence(size_t index, Ptr<Request> request)
    : index_(index), request_(request) {}

size_t RequestSentence::numTokens() const {
  return (request_->segmentTokens(index_));
}

void RequestSentence::completeSentence(Ptr<History> history) {
  // Relays completeSentence into request's processHistory, using index
  // information.
  request_->processHistory(index_, history);
}

Segment RequestSentence::getUnderlyingSegment() const {
  return request_->getSegment(index_);
}

bool operator<(const RequestSentence &a, const RequestSentence &b) {
  // Operator overload for usage in priority-queue / set.
  if (a.request_ == b.request_) {
    return a.index_ < b.index_;
  }
  return a.request_ < b.request_;
}

// ----------------------------------------------------------------------

} // namespace bergamot
} // namespace marian
