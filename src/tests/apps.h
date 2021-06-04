#ifndef BERGAMOT_APP_TESTAPP_H
#define BERGAMOT_APP_TESTAPP_H
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
Response translate_from_stdin(Ptr<Options> options, ResponseOptions responseOptions);

// Reads from stdin and translates. The quality score for the translations (each sentence) are printed separated by
// empty-lines. The first line contains whole quality scores and the second line word quality scores, for each entry.
void quality_scores(Ptr<Options> options);

// Reads from stdin and translates. Alignments are printed aligned to the source-tokens, following format src-token:
// [possible-target-alignments], if numeric is false. If numeric is true, only alignment probabilities are printed
// instead of the tokens.
void alignment_aggregated_to_source(Ptr<Options> options, bool numeric = false);

// Reads from stdin and translates.  Prints the tokens separated by space for each sentence.
void annotated_text_words(Ptr<Options> options, bool source = true);

// Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response in
// each line, depending on source = true or false.
void annotated_text_sentences(Ptr<Options> options, bool source = true);

// The output of old service-cli, all alignments, quality-scores, sentences in one app. This can be helpful for
// debugging purposes. The functions above are separated from what was previously legacy_service_cli.
void legacy_service_cli(Ptr<Options> options);

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_APP_TESTAPP_H
