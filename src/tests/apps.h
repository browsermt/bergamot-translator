#ifndef BERGAMOT_SRC_TESTS_APPS_H
#define BERGAMOT_SRC_TESTS_APPS_H
#include <algorithm>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"

namespace marian {
namespace bergamot {

namespace testapp {

// Utility function, common for all testapps. Reads content from stdin, builds a Service based on options and constructs
// a response containing translation data according responseOptions.
Response translateFromStdin(Ptr<Options> options, ResponseOptions responseOptions);

// Reads from stdin and translates.  Prints the tokens separated by space for each sentence. Prints words from source
// side text annotation if source=true, target annotation otherwise.
void annotatedTextWords(Ptr<Options> options, bool source = true);

// Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response
// in each line, depending on source = true or false respectively.
void annotatedTextSentences(Ptr<Options> options, bool source = true);

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_SRC_TESTS_APPS_H
