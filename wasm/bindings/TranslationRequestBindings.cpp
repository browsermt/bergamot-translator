/*
 * Bindings for TranslationRequest class
 *
 */

#include <emscripten/bind.h>

#include "response_options.h"

typedef marian::bergamot::ResponseOptions TranslationRequest;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_request) {
  enum_<marian::bergamot::ConcatStrategy>("ConcatStrategy")
      .value("FAITHFUL", marian::bergamot::ConcatStrategy::FAITHFUL)
      .value("SPACE", marian::bergamot::ConcatStrategy::SPACE);

  enum_<marian::bergamot::QualityScoreType>("QualityScoreType")
      .value("FREE", marian::bergamot::QualityScoreType::FREE)
      .value("EXPENSIVE", marian::bergamot::QualityScoreType::EXPENSIVE);

  value_object<TranslationRequest>("TranslationRequest")
      .field("qualityScores", &TranslationRequest::qualityScores)
      .field("alignment", &TranslationRequest::alignment)
      .field("alignmentThreshold", &TranslationRequest::alignmentThreshold)
      .field("sentenceMappings", &TranslationRequest::sentenceMappings)
      .field("qualityScoreType", &TranslationRequest::qualityScoreType)
      .field("concatStrategy", &TranslationRequest::concatStrategy);
}
