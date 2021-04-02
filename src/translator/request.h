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

/// A Request is an internal representation used to represent a Request after
/// processed by TextProcessor into sentences consituted by marian::Words.
///
/// The batching mechanism (Batcher) draws from multiple Requests and compiles
/// sentences into a batch. When a batch completes translation (at
/// BatchTranslator, intended in a different thread), backward propogation
/// happens through:
///
///   Batch::completeBatch(...)
//        -> RequestSentence::completeSentence(..)
///          -> Request::processHistory(...)
///
/// When all sentences in a Request are completed, responseBuilder is
/// triggered with the compiled Histories, to construct the Response
/// corresponding to the Request and set value of the promise which triggers the
/// future at Client.
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

  // Obtain the count of tokens in the segment correponding to index. Used to
  // insert sentence from multiple requests into the corresponding size bucket.
  size_t segmentTokens(size_t index) const;

  // Obtain number of segments in a request.
  size_t numSegments() const;

  // Obtains segment corresponding to index  to create a batch of segments among
  // several requests.
  Segment getSegment(size_t index) const;

  // For notions of priority among requests, used to enable std::set in
  // Batcher.
  bool operator<(const Request &request) const;

  // Processes a history obtained after translating in a heterogenous batch
  // compiled from requests.
  void processHistory(size_t index, Ptr<History> history);

  // On completion of last segment, sets value of the promise.
  // void completeRequest();

private:
  size_t Id_;
  size_t lineNumberBegin_;

  // Multiple translation-workers can concurrently access the same Request. The
  // following atomic atomically operates on the variable holding sentences
  // remaining to be translated.
  std::atomic<int> counter_;

  // source_ holds the source string to be translated. segments_ hold the
  // sentences generated from source_ in vector<Words>. sourceRanges_ are
  // string_views of the text corresponding to these words, pointing to
  // sequences in source_. histories_ is a buffer which eventually stores the
  // translations of each segment in the corresponding index.
  // AnnotatedText source_;
  Segments segments_;
  std::vector<Ptr<History>> histories_;

  // Members above are moved into newly constructed Response on completion
  // of translation of all segments. The promise below is set to this Response
  // value. future to this promise is made available to the user through
  // Service.
  // std::promise<Response> response_;

  // Constructing Response requires the vocabs_ used to generate Request.
  // std::vector<Ptr<Vocab const>> *vocabs_;
  ResponseBuilder responseBuilder_;
};

class RequestSentence {
  // A RequestSentence provides a view to a sentence within a Request. Existence
  // of this class allows the sentences and associated information to be kept
  // within Request.

public:
  RequestSentence(size_t, Ptr<Request>);
  size_t numTokens() const;

  // lineNumber in Request, used for matching marian-decoder. SentenceTuple
  // requires lineNumber to be set for Corpus based batches.
  size_t lineNumber() const;

  // Accessor to the segment represented by the RequestSentence.
  Segment getUnderlyingSegment() const;

  // Forwards call to Request, checking for completion.
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
