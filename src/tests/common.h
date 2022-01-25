#pragma once
#include <algorithm>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"
#include "translator/utils.h"

namespace marian::bergamot {

/// Due to the stubborn-ness of the extension and native to not agree on API (e.g, translateMultiple vs translate),
/// different underlying cache we have the following "bridge" at test-applications - taking into account the fact that
/// the most commonly used primitives across both Services is a single text blob in and corresponding Response out, in a
/// blocking fashion.
///
/// The following contraption constrains a single sentence to single Response parameterized by Service, in a test-suite
/// below. This allows sharing of code for test-suite between WebAssembly's workflows and Native's workflows.
///
/// The intention here is to use templating to achieve the same thing an ifdef would have at compile-time. Also mandates
/// after bridge layer, both WebAssembly and Native paths compile correctly (this does not guarantee outputs are the
/// same through both code-paths, or that both are tested at runtime - only that both compile and work with a bridge).
///
/// For any complex workflows involving non-blocking concurrent translation, it is required to write something not
/// constrained by the following.

template <class Service>
struct Bridge : public std::false_type {};

template <>
struct Bridge<BlockingService> : public std::true_type {
  Response translate(BlockingService &service, std::shared_ptr<TranslationModel> &model, std::string &&source,
                     const ResponseOptions &responseOptions);
  Response pivot(BlockingService &service, std::shared_ptr<TranslationModel> &sourceToPivot,
                 std::shared_ptr<TranslationModel> &pivotToTarget, std::string &&source,
                 const ResponseOptions &responseOptions);
};

template <>
struct Bridge<AsyncService> : public std::true_type {
  Response translate(AsyncService &service, std::shared_ptr<TranslationModel> &model, std::string &&source,
                     const ResponseOptions &responseOptions);
  Response pivot(AsyncService &service, std::shared_ptr<TranslationModel> &sourceToPivot,
                 std::shared_ptr<TranslationModel> &pivotToTarget, std::string &&source,
                 const ResponseOptions &responseOptions);
};

template <class Service>
class TestSuite {
 private:
  Bridge<Service> bridge_;
  Service &service_;

 public:
  TestSuite(Service &service);
  void run(const std::string &opModeAsString, std::vector<Ptr<TranslationModel>> &models);

 private:
  void benchmarkDecoder(Ptr<TranslationModel> &model);

  // Reads from stdin and translates.  Prints the tokens separated by space for each sentence. Prints words from source
  // side text annotation if source=true, target annotation otherwise.
  void annotatedTextWords(Ptr<TranslationModel> model, bool sourceSide = true);

  // Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response
  // in each line, depending on source = true or false respectively.
  void annotatedTextSentences(Ptr<TranslationModel> model, bool sourceSide = true);

  void forwardAndBackward(std::vector<Ptr<TranslationModel>> &models);

  // Reads from stdin and translates the read content. Prints the quality words for each sentence.
  void qualityEstimatorWords(Ptr<TranslationModel> model);

  // Reads from stdin and translates the read content. Prints the quality scores for each sentence.
  void qualityEstimatorScores(Ptr<TranslationModel> model);

  void translationCache(Ptr<TranslationModel> model);

  void pivotTranslate(std::vector<Ptr<TranslationModel>> &models);

  void htmlTranslation(Ptr<TranslationModel> model);
};

#define BERGAMOT_TESTS_COMMON_IMPL
#include "common-impl.cpp"
#undef BERGAMOT_TESTS_COMMON_IMPL

}  // namespace marian::bergamot
