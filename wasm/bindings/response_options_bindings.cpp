/*
 * Bindings for ResponseOptions class
 *
 */

#include <emscripten/bind.h>

#include "response_options.h"

typedef marian::bergamot::ResponseOptions ResponseOptions;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(response_options) { class_<ResponseOptions>("ResponseOptions").constructor<>(); }
