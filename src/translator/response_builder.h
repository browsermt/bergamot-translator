#pragma once
#include "response.h"

namespace marian {
namespace bergamot {

/// ResponseBuilder is a callback functor. It is expected to be bound to a
/// Request after giving it the context of options, vocabs and promise to set.
/// It constructs the Response and it's members based on options
/// (quality=on|off, alignments=on|off, mappings=on|off, splitmode=sentence |
/// paragraph).

class ResponseBuilder {
public:
  /// @param [in] options: marian Options object (used to determine operating
  /// mode)
  /// @param [in] vocabs: marian vocab object (used in decoding)
  //  @param [in] promise: promise to set with the constructed Response
  ResponseBuilder(Ptr<Options> options, AnnotatedText &&source,
                  std::vector<Ptr<Vocab const>> &vocabs,
                  std::promise<Response> &&promise)
      : options_(options), source_(std::move(source)), vocabs_(&vocabs),
        promise_(std::move(promise)) {}

  /// Constructs and sets the promise of a Response object from obtained
  /// histories after translating.
  /// @param [in] histories: Histories obtained after translating the Request
  /// from which this functor is called.
  void operator()(Histories &&histories) {
    // TODO(jerinphilip) load RequestParams into options and turn build
    // functions on or off.
    //
    ABORT_IF(source_.size() != histories.size());
    Response response;

    // Move source_ into response.
    response.source_ = std::move(source_);

    buildTranslatedText(histories, response);

    // Should always be after buildTranslatedText
    buildQualityScores(histories, response);

    buildAlignments(histories, response);

    // Once complete, set promise.
    promise_.set_value(std::move(response));
  }

private:
  /// Builds qualityScores from histories and writes to response. expects
  /// buildTranslatedText to be run before to be able to obtain target text and
  /// subword information.
  /// @param histories [in]
  /// @param response [out]
  void buildQualityScores(Histories &histories, Response &response);

  /// Builds alignments from histories and writes onto response.
  /// @param histories [in]
  /// @param response [out]
  void buildAlignments(Histories &histories, Response &response);

  /// Builds translated text and subword annotations and writes onto response.
  /// @param histories [in]
  /// @param response [out]
  void buildTranslatedText(Histories &histories, Response &response);

  // Data members are context/curried args for the functor.

  Ptr<Options> options_;
  std::vector<Ptr<Vocab const>> *vocabs_; // vocabs are required for decoding
                                          // and any source validation checks.
  std::promise<Response> promise_; //  To be set when callback triggered and
                                   //  after Response constructed.
  AnnotatedText source_;
};
} // namespace bergamot
} // namespace marian
