#include "request.h"

#include <string>

#include "annotation.h"
#include "common/logging.h"
#include "definitions.h"
#include "response.h"

namespace marian {
namespace bergamot {

// -----------------------------------------------------------------
Request::Request(size_t Id, Segments &&segments, ResponseBuilder &&responseBuilder, TranslationCache *cache)
    : Id_(Id),
      segments_(std::move(segments)),
      responseBuilder_(std::move(responseBuilder)),
      cache_(cache)

{
  counter_ = segments_.size();
  processedRequestSentences_.resize(segments_.size());

  if (cache_ != nullptr) {
    for (size_t idx = 0; idx < segments_.size(); idx++) {
      ProcessedRequestSentence processedRequestSentence;
      if (cache_->fetch(getSegment(idx), processedRequestSentence)) {
        processedRequestSentences_[idx] = processedRequestSentence;
        --counter_;
      }
    }
  }

  // If there are no segments_, we are never able to trigger the responseBuilder calls from a different thread. However,
  // in this case we want an empty valid response. Also, if cache somehow manages to decrease all counter prefilling
  // histories, then we'd have to trigger ResponseBuilder as well. No segments go into batching and therefore no
  // processHistory triggers.
  if (segments_.size() == 0 || counter_.load() == 0) {
    responseBuilder_(std::move(processedRequestSentences_));
  }
}

bool Request::isCachePrefilled(size_t index) { return !(processedRequestSentences_[index].empty()); }

size_t Request::numSegments() const { return segments_.size(); }

size_t Request::segmentTokens(size_t index) const { return (segments_[index].size()); }

Segment Request::getSegment(size_t index) const { return segments_[index]; }

void Request::processHistory(size_t index, Ptr<History> history) {
  // Concurrently called by multiple workers as a history from translation is
  // ready. The container storing histories is set with the value obtained.
  processedRequestSentences_[index] = ProcessedRequestSentence(*history);
  if (cache_ != nullptr) {
    cache_->insert(getSegment(index), processedRequestSentences_[index]);
  }

  // In case this is last request in, completeRequest is called, which sets the
  // value of the promise.
  if (--counter_ == 0) {
    responseBuilder_(std::move(processedRequestSentences_));
  }
}

bool Request::operator<(const Request &b) const {
  // Among Requests, only sequence id is used for obtaining priority.
  return Id_ < b.Id_;
}

// ------------------------------------------------------------------

RequestSentence::RequestSentence(size_t index, Ptr<Request> request) : index_(index), request_(request) {}

size_t RequestSentence::numTokens() const { return (request_->segmentTokens(index_)); }

void RequestSentence::completeSentence(Ptr<History> history) {
  // Relays completeSentence into request's processHistory, using index
  // information.
  request_->processHistory(index_, history);
}

Segment RequestSentence::getUnderlyingSegment() const { return request_->getSegment(index_); }

bool operator<(const RequestSentence &a, const RequestSentence &b) {
  // Operator overload for usage in priority-queue / set.
  if (a.request_ == b.request_) {
    return a.index_ < b.index_;
  }
  return a.request_ < b.request_;
}

bool RequestSentence::isCachePrefilled() { return request_->isCachePrefilled(index_); }

// ----------------------------------------------------------------------

}  // namespace bergamot
}  // namespace marian
