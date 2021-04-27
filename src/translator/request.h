#ifndef SRC_BERGAMOT_REQUEST_H_
#define SRC_BERGAMOT_REQUEST_H_

#include "definitions.h"
#include "response.h"
#include "response_builder.h"
#include "sentence_ranges.h"

#include "common/logging.h"
#include "data/types.h"
#include "translator/beam_search.h"

#include <cassert>

#include <future>
#include <vector>

namespace marian {
namespace bergamot {

/// A Request is an internal representation used to represent a request after
/// processed by TextProcessor into sentences constituted by marian::Words.
///
/// The batching mechanism (Batcher) draws from multiple Requests and compiles
/// sentences into a batch. When a batch completes translation (at
/// BatchTranslator, intended in a different thread), backward propogation
/// happens through:
///
/// ```cpp
///   Batch::completeBatch(...)
///       -> RequestSentence::completeSentence(..)
///          -> Request::processHistory(...)
/// ```
///
/// When all sentences in a Request are completed, responseBuilder is
/// triggered with the compiled Histories, to construct the Response
/// corresponding to the Request and set value of the promise which triggers the
/// future at client.
class Request {
public:
  /// Constructs an internal representation of the Request identified by Id,
  /// processed Segments and accepts a callback (ResponseBuilder) which builds
  /// the Response upon completion of the Request.
  ///
  ///
  /// @param [in] Id: Identifier assigned to Request by Service.
  /// @param [in] segments: Each segment is a unit to be translated.
  /// @param [in] responseBuilder: Callback function (of ResponseBuilder type)
  /// to be triggered upon the completion of translation of all units in a
  /// Request.
  Request(size_t Id, Segments &&segments, ResponseBuilder &&responseBuilder);

  /// Obtain the count of tokens in the segment correponding to index. Used to
  /// insert sentence from multiple requests into the corresponding size bucket.
  size_t segmentTokens(size_t index) const;

  /// Obtain number of segments in a request.
  size_t numSegments() const;

  /// Obtains segment corresponding to index  to create a batch of segments
  /// among several requests.
  Segment getSegment(size_t index) const;

  /// For notions of priority among requests, used to enable std::set in
  /// Batcher.
  bool operator<(const Request &request) const;

  /// Processes a history obtained after translating in a heterogenous batch
  /// compiled from requests.
  void processHistory(size_t index, Ptr<History> history);

private:
  size_t Id_;

  /// Multiple translation-workers can concurrently access the same Request. The
  /// following atomic atomically operates on the variable holding sentences
  /// remaining to be translated.
  std::atomic<int> counter_;

  /// segments_ hold the sentences processed into Words which generated from
  /// input string.
  Segments segments_;

  /// histories_ is a buffer which eventually stores the translations of each
  /// segment in the corresponding index.
  std::vector<Ptr<History>> histories_;

  /// Constructing Response requires the vocabs_ used to generate Request.
  /// std::vector<Ptr<Vocab const>> *vocabs_;
  ResponseBuilder responseBuilder_;
};

/// A RequestSentence provides a view to a sentence within a Request. Existence
/// of this class allows the sentences and associated information to be kept
/// within Request, while batching mechanism (Batcher) compiles Batch from
/// RequestSentence-s coming from different Requests.
class RequestSentence {

public:
  RequestSentence(size_t, Ptr<Request>);

  /// Number of tokens in the segment this RequestSentence represents. Used to
  /// order by length in batching.
  size_t numTokens() const;

  /// Accessor to the segment represented by the RequestSentence.
  Segment getUnderlyingSegment() const;

  /// Forwards history to Request to set history corresponding to this
  /// RequestSentence.
  void completeSentence(Ptr<History> history);

  friend bool operator<(const RequestSentence &a, const RequestSentence &b);

private:
  size_t index_;
  Ptr<Request> request_;
};

typedef std::vector<RequestSentence> RequestSentences;

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_REQUEST_H_
