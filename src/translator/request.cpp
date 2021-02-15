#include "request.h"

#include "definitions.h"
#include "response.h"

#include "common/logging.h"

#include <string>

namespace marian {
namespace bergamot {

// -----------------------------------------------------------------
Request::Request(unsigned int Id, int lineNumberBegin,
                 std::vector<Ptr<Vocab const>> &vocabs, std::string &&source,
                 Segments &&segments,
                 std::vector<TokenRanges> &&sourceTokenRanges,
                 std::promise<Response> responsePromise)
    : Id_(Id), lineNumberBegin_(lineNumberBegin), vocabs_(&vocabs),
      source_(std::move(source)), segments_(std::move(segments)),
      sourceTokenRanges_(std::move(sourceTokenRanges)),
      response_(std::move(responsePromise)) {

  counter_ = segments_.size();
  histories_.resize(segments_.size(), nullptr);
}

size_t Request::lineNumberBegin() const { return lineNumberBegin_; }
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
    completeRequest();
  }
}

void Request::completeRequest() {
  // Request no longer needs to hold the content, can transfer it to
  // Response.
  Response response(std::move(source_), std::move(sourceTokenRanges_),
                    std::move(histories_), *vocabs_);
  response_.set_value(std::move(response));
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

size_t RequestSentence::lineNumber() const {
  return (request_->lineNumberBegin() + index_);
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

void Batch::reset() {
  Id_ = 0;
  sentences_.clear();
}

void Batch::log() {
  int numTokens{0}, maxLength{0};
  for (auto &sentence : sentences_) {
    numTokens += sentence.numTokens();
    maxLength = std::max(maxLength, static_cast<int>(sentence.numTokens()));
  }

  LOG(info, "Batch(Id_={}, tokens={}, max-length={}, sentences_={})", Id_,
      numTokens, maxLength, sentences_.size());
}

void Batch::add(const RequestSentence &sentence) {
  sentences_.push_back(sentence);
}

void Batch::setId(int Id) {
  assert(Id > 0);
  Id_ = Id;
  if (Id % 500 == 0) {
    log();
  }
}

void Batch::completeBatch(const Histories &histories) {
  for (int i = 0; i < sentences_.size(); i++) {
    sentences_[i].completeSentence(histories[i]);
  }
}

} // namespace bergamot
} // namespace marian
