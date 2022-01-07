/*
 * Bindings for ResponseOptions class
 *
 */

#include <emscripten/bind.h>

#include "response_options.h"

using ResponseOptions = marian::bergamot::ResponseOptions;

using namespace emscripten;

EMSCRIPTEN_BINDINGS(response_options) {
  value_object<ResponseOptions>("ResponseOptions")
      .field("qualityScores", &ResponseOptions::qualityScores)
      .field("alignment", &ResponseOptions::alignment)
      .field("html", &ResponseOptions::HTML)
      .field("htmlVoidTags", &ResponseOptions::HTMLVoidTags)
      .field("htmlInlineTags", &ResponseOptions::HTMLInlineTags)
      .field("htmlContinuationDelimiters", &ResponseOptions::HTMLContinuationDelimiters);
}
