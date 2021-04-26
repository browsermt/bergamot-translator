#ifndef SRC_BERGAMOT_RESPONSE_BUILDER_H_
#define SRC_BERGAMOT_RESPONSE_BUILDER_H_

#include "data/types.h"
#include "response.h"
#include "response_options.h"

// For now we will work with this, to avoid complaints another structure is hard
// to operate with.

namespace marian {
namespace bergamot {

/// ResponseBuilder is a callback functor. It is expected to be bound to a
/// Request after giving it the context of options, vocabs and promise to set.
/// It constructs the Response and it's members based on options
/// (quality=on|off, alignments=on|off, mappings=on|off, splitmode=sentence |
/// paragraph).

class ResponseBuilder {
public:
  /// @param [in] responseOptions: ResponseOptions, indicating what to include
  /// or not in the response and any additional configurable parameters.
  /// @param [in] vocabs: marian vocab object (used in decoding)
  /// @param [in] promise: promise to set with the constructed Response.
  ResponseBuilder(ResponseOptions responseOptions, AnnotatedText &&source,
                  std::vector<Ptr<Vocab const>> &vocabs,
                  std::promise<Response> &&promise)
      : responseOptions_(responseOptions), source_(std::move(source)),
        vocabs_(&vocabs), promise_(std::move(promise)) {}

  /// Constructs and sets the promise of a Response object from obtained
  /// histories after translating.
  /// @param [in] histories: Histories obtained after translating the Request
  /// from which this functor is called.
  void operator()(Histories &&histories) {
    // TODO(jerinphilip) load ResponseOptions into options and turn build
    // functions on or off.
    // responseOptions_ is unused, but we can try something here.
    ABORT_IF(source_.numSentences() != histories.size(),
             "Mismatch in source and translated sentences");
    Response response;

    // Move source_ into response.
    response.source = std::move(source_);

    // Should be after source is set
    buildTranslatedText(histories, response);

    // Should always be after buildTranslatedText
    if (responseOptions_.qualityScores) {
      buildQualityScores(histories, response);
    }

    if (responseOptions_.alignment) {
      buildAlignments(histories, response);
    }

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

  ResponseOptions responseOptions_;
  std::vector<Ptr<Vocab const>> *vocabs_; // vocabs are required for decoding
                                          // and any source validation checks.
  std::promise<Response> promise_; //  To be set when callback triggered and
                                   //  after Response constructed.
  AnnotatedText source_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_RESPONSE_BUILDER_H_
