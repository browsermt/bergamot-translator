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
                     const ResponseOptions &responseOptions) {
    // project source to a vector of std::string, send in, unpack the first element from
    // vector<Response>, return.
    std::vector<std::string> sources = {source};
    return service.translateMultiple(model, std::move(sources), responseOptions).front();
  }
};

template <>
struct Bridge<AsyncService> : public std::true_type {
  Response translate(AsyncService &service, std::shared_ptr<TranslationModel> &model, std::string &&source,
                     const ResponseOptions &responseOptions) {
    // downgrade to blocking via promise, future, wait and return response;
    std::promise<Response> responsePromise;
    std::future<Response> responseFuture = responsePromise.get_future();

    auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
    service.translate(model, std::move(source), callback, responseOptions);

    responseFuture.wait();

    Response response = responseFuture.get();
    return response;
  }
};

template <class Service>
class TestSuite {
 private:
  Bridge<Service> bridge_;
  Service &service_;

  enum OpMode {
    APP_NATIVE,
    TEST_BENCHMARK_DECODER,
    TEST_WASM_PATH,
    TEST_SOURCE_SENTENCES,
    TEST_TARGET_SENTENCES,
    TEST_SOURCE_WORDS,
    TEST_TARGET_WORDS,
    TEST_QUALITY_ESTIMATOR_WORDS,
    TEST_QUALITY_ESTIMATOR_SCORES,
    TEST_FORWARD_BACKWARD_FOR_OUTBOUND,
    TEST_TRANSLATION_CACHE,
  };

  const std::unordered_map<std::string, OpMode> testAppRegistry_;

 public:
  TestSuite(Service &service)
      : service_{service},
        testAppRegistry_{
            {"native", OpMode::APP_NATIVE},
            {"wasm", OpMode::TEST_WASM_PATH},
            {"decoder", OpMode::TEST_BENCHMARK_DECODER},
            {"test-response-source-sentences", OpMode::TEST_SOURCE_SENTENCES},
            {"test-response-target-sentences", OpMode::TEST_TARGET_SENTENCES},
            {"test-response-source-words", OpMode::TEST_SOURCE_WORDS},
            {"test-response-target-words", OpMode::TEST_TARGET_WORDS},
            {"test-quality-estimator-words", OpMode::TEST_QUALITY_ESTIMATOR_WORDS},
            {"test-quality-estimator-scores", OpMode::TEST_QUALITY_ESTIMATOR_SCORES},
            {"test-forward-backward", OpMode::TEST_FORWARD_BACKWARD_FOR_OUTBOUND},
            {"test-translation-cache", OpMode::TEST_TRANSLATION_CACHE},
        } {}

  void run(const std::string &opModeAsString, std::vector<Ptr<TranslationModel>> &models) {
    auto query = testAppRegistry_.find(opModeAsString);
    if (query != testAppRegistry_.end()) {
      switch (query->second) {
        case OpMode::TEST_BENCHMARK_DECODER:
          benchmarkDecoder(models.front());
          break;
        case OpMode::TEST_SOURCE_SENTENCES:
          annotatedTextSentences(models.front(), /*source=*/true);
          break;
        case OpMode::TEST_TARGET_SENTENCES:
          annotatedTextSentences(models.front(), /*source=*/false);
          break;
        case OpMode::TEST_SOURCE_WORDS:
          annotatedTextWords(models.front(), /*source=*/true);
          break;
        case OpMode::TEST_TARGET_WORDS:
          annotatedTextWords(models.front(), /*source=*/false);
          break;
        case OpMode::TEST_FORWARD_BACKWARD_FOR_OUTBOUND:
          forwardAndBackward(models);
          break;
        case OpMode::TEST_QUALITY_ESTIMATOR_WORDS:
          qualityEstimatorWords(models.front());
          break;
        case OpMode::TEST_QUALITY_ESTIMATOR_SCORES:
          qualityEstimatorScores(models.front());
          break;
        case OpMode::TEST_TRANSLATION_CACHE:
          translationCache(models.front());
          break;
        default:
          // Never happens.
          break;
      }

    } else {
      std::cerr << "Incompatible test mode. Choose from the following test-modes:\n";
      for (auto &p : testAppRegistry_) {
        std::cerr << "  " << p.first << "\n";
      }

      std::abort();
    }
  }

 private:
  void benchmarkDecoder(Ptr<TranslationModel> &model) {
    marian::timer::Timer decoderTimer;
    std::string source = readFromStdin();

    ResponseOptions responseOptions;
    Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

    for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
      std::cout << response.target.sentence(sentenceIdx) << "\n";
    }

    std::cerr << "Total time: " << std::setprecision(5) << decoderTimer.elapsed() << "s wall" << std::endl;
  }

  // Reads from stdin and translates.  Prints the tokens separated by space for each sentence. Prints words from source
  // side text annotation if source=true, target annotation otherwise.
  void annotatedTextWords(Ptr<TranslationModel> model, bool sourceSide = true) {
    ResponseOptions responseOptions;
    std::string source = readFromStdin();
    Response response = bridge_.translate(service_, model, std::move(source), responseOptions);
    AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
    for (size_t s = 0; s < annotatedText.numSentences(); s++) {
      for (size_t w = 0; w < annotatedText.numWords(s); w++) {
        std::cout << (w == 0 ? "" : "\t");
        std::cout << annotatedText.word(s, w);
      }
      std::cout << "\n";
    }
  }

  // Reads from stdin and translates the read content. Prints the sentences in source or target in constructed response
  // in each line, depending on source = true or false respectively.
  void annotatedTextSentences(Ptr<TranslationModel> model, bool sourceSide = true) {
    ResponseOptions responseOptions;
    std::string source = readFromStdin();
    Response response = bridge_.translate(service_, model, std::move(source), responseOptions);
    AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
    for (size_t s = 0; s < annotatedText.numSentences(); s++) {
      std::cout << annotatedText.sentence(s) << "\n";
    }
  }

  void forwardAndBackward(std::vector<Ptr<TranslationModel>> &models) {
    ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
    ResponseOptions responseOptions;
    std::string source = readFromStdin();
    Response forwardResponse = bridge_.translate(service_, models.front(), std::move(source), responseOptions);

    // Make a copy of target
    std::string target = forwardResponse.target.text;
    Response backwardResponse = bridge_.translate(service_, models.back(), std::move(target), responseOptions);

    // Print both onto the command-line
    std::cout << forwardResponse.source.text;
    std::cout << "----------------\n";
    std::cout << forwardResponse.target.text;
    std::cout << "----------------\n";
    std::cout << backwardResponse.target.text;
  }

  // Reads from stdin and translates the read content. Prints the quality words for each sentence.
  void qualityEstimatorWords(Ptr<TranslationModel> model) {
    ResponseOptions responseOptions;
    responseOptions.qualityScores = true;
    std::string source = readFromStdin();
    const Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

    for (const auto &sentenceQualityEstimate : response.qualityScores) {
      std::cout << "[SentenceBegin]\n";

      for (const auto &wordByteRange : sentenceQualityEstimate.wordByteRanges) {
        const string_view word(response.target.text.data() + wordByteRange.begin, wordByteRange.size());
        std::cout << word << "\n";
      }
      std::cout << "[SentenceEnd]\n\n";
    }
  }

  // Reads from stdin and translates the read content. Prints the quality scores for each sentence.
  void qualityEstimatorScores(Ptr<TranslationModel> model) {
    ResponseOptions responseOptions;
    responseOptions.qualityScores = true;

    std::string source = readFromStdin();
    const Response response = bridge_.translate(service_, model, std::move(source), responseOptions);

    for (const auto &sentenceQualityEstimate : response.qualityScores) {
      std::cout << std::fixed << std::setprecision(3) << sentenceQualityEstimate.sentenceScore << "\n";

      for (const float &wordScore : sentenceQualityEstimate.wordScores) {
        std::cout << std::fixed << std::setprecision(3) << wordScore << "\n";
      }
      std::cout << "\n";
    }
  }

  void translationCache(Ptr<TranslationModel> model) {
    ResponseOptions responseOptions;

    // Read a large input text blob from stdin
    const std::string source = readFromStdin();

    // Round 1
    std::string buffer = source;
    Response firstResponse = bridge_.translate(service_, model, std::move(buffer), responseOptions);

    auto statsFirstRun = service_.cacheStats();
    LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
    ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

    // Round 2; There should be cache hits
    buffer = source;
    Response secondResponse = bridge_.translate(service_, model, std::move(buffer), responseOptions);

    auto statsSecondRun = service_.cacheStats();
    LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
    ABORT_IF(statsSecondRun.hits <= 0, "At least one hit expected, none found.");
    if (statsSecondRun.hits != statsFirstRun.misses) {
      std::cerr << "Mismatch in expected hits (Hits, Misses = " << statsSecondRun.hits << ", " << statsSecondRun.misses
                << "). This can happen due to random eviction." << std::endl;
    }

    ABORT_IF(firstResponse.target.text != secondResponse.target.text,
             "Recompiled string provided different output when operated with cache. On the same hardware while using "
             "same path, this is expected to be same.");

    std::cout << firstResponse.target.text;
  }
};  // namespace marian::bergamot

}  // namespace marian::bergamot
