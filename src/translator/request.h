//
// Defines:
//
// Request: holds the input blob of a text, Segments (vector<Words>) which are
// to go to the batching mechanism and alignments between the processed
// segments and the input blob (sourceAlignments). In addition, Request takes
// care of the barrier which fires when all the Segments in a request are done
// translating by the workers (BatchTranslator). Request is to be extended with
// notions of Priority (sequence, user-given).
//
// RequestSentence: is a tuple of (index, Request*). This provides the
// batching mechanism access to the segment within the request. The backref to
// Request allows event triggering the barrier upon completion of the last
// sentence by a worker.
//
// PCItem: is a vector of RequestSentences and a batchNumber, which is what the
// PCQueue holds. The batches are constructed from segments returned by a
// RequestSentence. Can be enhanced with paddingSize, countTokens eventually for
// logging.

#ifndef SRC_BERGAMOT_REQUEST_H_
#define SRC_BERGAMOT_REQUEST_H_

#include "definitions.h"
#include "translation_result.h"

#include "common/logging.h"
#include "data/types.h"
#include "translator/beam_search.h"

#include <cassert>

#include <future>
#include <vector>

namespace marian {
namespace bergamot {

class Request {
private:
  unsigned int Id_;
  int lineNumberBegin_;
  std::string source_;
  std::atomic<int> counter_;
  std::vector<Ptr<Vocab const>> *vocabs_;

  Segments segments_;
  std::vector<TokenRanges> sourceAlignments_;
  std::vector<Ptr<History>> histories_;

  std::promise<Response> response_;

public:
  Request(unsigned int Id, int lineNumberBegin,
          std::vector<Ptr<Vocab const>> &vocabs_, std::string &&source,
          Segments &&segments, std::vector<TokenRanges> &&sourceAlignments,
          std::promise<Response> responsePromise);

  // Obtain the count of tokens in the segment correponding to index. Used to
  // insert sentence from multiple requests into the corresponding size bucket.
  size_t segmentTokens(size_t index) const;

  // Obtain number of segments in a request.
  size_t numSegments() const;
  size_t lineNumberBegin() const;

  // Obtains segment corresponding to index  to create a batch of segments among
  // several requests.
  Segment getSegment(size_t index) const;

  // For notions of priority among requests (used to enable <set> in Batcher).
  bool operator<(const Request &request) const;

  // Processes a history obtained after translating in a heterogenous batch
  // compiled from requests.
  void processHistory(size_t index, Ptr<History> history);

  // On completion of last segment, sets value of the promise.
  void completeRequest();
};

class RequestSentence {
private:
  size_t index_;
  Ptr<Request> request_;

public:
  RequestSentence(size_t, Ptr<Request>);
  size_t numTokens() const;
  size_t lineNumber() const;
  Segment getUnderlyingSegment() const;
  void completeSentence(Ptr<History> history);
  friend bool operator<(const RequestSentence &a, const RequestSentence &b);
};

typedef std::vector<RequestSentence> RequestSentences;

class Batch {
public:
  Batch() { reset(); }
  void reset() {
    Id_ = 0;
    sentences_.clear();
  }
  // Convenience function to determine poison.
  bool isPoison() { return (Id_ == -1); }
  static Batch poison() {
    Batch poison_;
    poison_.Id_ = -1;
    return poison_;
  }

  void log() {
    int numTokens{0}, maxLength{0};
    for (auto &sentence : sentences_) {
      numTokens += sentence.numTokens();
      maxLength = std::max(maxLength, static_cast<int>(sentence.numTokens()));
    }

    LOG(info, "Batch(Id_={}, tokens={}, max-length={}, sentences_={})", Id_,
        numTokens, maxLength, sentences_.size());
  }

  void add(const RequestSentence &sentence) { sentences_.push_back(sentence); }

  size_t size() { return sentences_.size(); }

  void setId(int Id) {
    assert(Id > 0);
    Id_ = Id;
    if (Id % 500 == 0) {
      log();
    }
  }

  const RequestSentences &sentences() { return sentences_; }
  void completeBatch(const Histories &histories) {
    for (int i = 0; i < sentences_.size(); i++) {
      sentences_[i].completeSentence(histories[i]);
    }
  }

private:
  int Id_;
  RequestSentences sentences_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_REQUEST_H_
