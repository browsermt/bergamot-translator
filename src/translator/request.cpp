#include "request.h"

#include <string>

#include "annotation.h"
#include "cache.h"
#include "common/logging.h"
#include "definitions.h"
#include "response.h"
#include "translation_model.h"

namespace marian {
namespace bergamot {

size_t hashForCache(const TranslationModel &model, const marian::Words &words) {
  size_t seed = 42;
  util::hash_combine<size_t>(seed, model.modelId());
  for (auto &word : words) {
    size_t hashWord = static_cast<size_t>(word.toWordIndex());
    util::hash_combine<size_t>(seed, hashWord);
  }
  return seed;
}

// -----------------------------------------------------------------
Request::Request(size_t Id, const TranslationModel &model, Segments &&segments, ResponseBuilder &&responseBuilder,
                 TranslationCache *cache)
    : Id_(Id),
      model_(model),
      segments_(std::move(segments)),
      responseBuilder_(std::move(responseBuilder)),
      cache_(cache) {
  counter_ = segments_.size();
  histories_.resize(segments_.size(), nullptr);

  // 1. If there are no segments_, we are never able to trigger the responseBuilder calls from a different thread. This
  // happens when the use provides empty input, or the sentence and subword preprocessing deems no translatable units
  // present. However, in this case we want an empty valid response. There's no need to do any additional processing
  // here.
  if (segments_.size() == 0) {
    responseBuilder_(std::move(histories_));
  } else {
    counter_ = segments_.size();
    histories_.resize(segments_.size());

    if (cache_ != nullptr) {
      // Iterate through segments, see if any can be prefilled from cache. If prefilled, mark the particular segments as
      // complete (non-empty ProcessedRequestSentence). Also update accounting used elsewhere (counter_) to reflect one
      // less segment to translate.
      for (size_t idx = 0; idx < segments_.size(); idx++) {
        size_t key = hashForCache(model_, getSegment(idx));
        auto [found, history] = cache_->find(key);
        if (found) {
          histories_[idx] = history;
          --counter_;
        }
      }
      // 2. Also, if cache somehow manages to decrease all counter prefilling histories, then we'd have to trigger
      // ResponseBuilder as well. No segments go into batching and therefore no processHistory triggers.
      if (counter_.load() == 0) {
        responseBuilder_(std::move(histories_));
      }
    }
  }
}

size_t Request::numSegments() const { return segments_.size(); }

size_t Request::segmentTokens(size_t index) const { return (segments_[index].size()); }

Segment Request::getSegment(size_t index) const { return segments_[index]; }

void Request::processHistory(size_t index, Ptr<History> history) {
  // Concurrently called by multiple workers as a history from translation is
  // ready. The container storing histories is set with the value obtained.

  // Fill in placeholder from History obtained by freshly translating. Since this was a cache-miss to have got through,
  // update cache if available to store the result.
  histories_[index] = history;
  if (cache_ != nullptr) {
    size_t key = hashForCache(model_, getSegment(index));
    cache_->store(key, histories_[index]);
  }

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

// ----------------------------------------------------------------------

}  // namespace bergamot
}  // namespace marian
