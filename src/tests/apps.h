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

// Reads from stdin and translates.  Prints the tokens separated by space for each sentence. Prints words from source
// side text annotation if source=true, target annotation otherwise.
void annotatedTextWords(AsyncService &service, Ptr<TranslationModel> model, bool source = true);

// Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response
// in each line, depending on source = true or false respectively.
void annotatedTextSentences(AsyncService &service, Ptr<TranslationModel> model, bool source = true);

void forwardAndBackward(AsyncService &service, std::vector<Ptr<TranslationModel>> &models);

// Reads from stdin and translates the read content. Prints the quality words for each sentence.
void qualityEstimatorWords(AsyncService &service, Ptr<TranslationModel> model);

// Reads from stdin and translates the read content. Prints the quality scores for each sentence.
void qualityEstimatorScores(AsyncService &service, Ptr<TranslationModel> model);

void translationCache(AsyncService &service, Ptr<TranslationModel> model);

void benchmarkCacheEditWorkflow(AsyncService &service, Ptr<TranslationModel> model);

void wngt20IncrementalDecodingForCache(AsyncService &service, Ptr<TranslationModel> model);

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_SRC_TESTS_APPS_H
