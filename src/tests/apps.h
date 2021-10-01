#pragma once
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

template <class Service, class SpecializedTranslateForResponse = TranslateForResponse<Service>>
class TestSuite {
 private:
  SpecializedTranslateForResponse translateForResponse_;
  Service &service_;

 public:
  TestSuite(Service &service) : service_(service), translateForResponse_() {}

  void benchmarkDecoder(Ptr<TranslationModel> &model) {
    marian::timer::Timer decoderTimer;
    std::string input = readFromStdin();

    // Wait on future until Response is complete
    std::promise<Response> responsePromise;
    std::future<Response> responseFuture = responsePromise.get_future();
    auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

    service_.translate(model, std::move(input), std::move(callback));
    responseFuture.wait();
    const Response &response = responseFuture.get();

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
    Response response = translateForResponse_(service_, model, std::move(source), responseOptions);
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
    Response response = translateForResponse_(service_, model, std::move(source), responseOptions);
    AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
    for (size_t s = 0; s < annotatedText.numSentences(); s++) {
      std::cout << annotatedText.sentence(s) << "\n";
    }
  }

  void forwardAndBackward(std::vector<Ptr<TranslationModel>> &models) {
    ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
    ResponseOptions responseOptions;
    std::string source = readFromStdin();
    Response forwardResponse = translateForResponse_(service_, models.front(), std::move(source), responseOptions);

    // Make a copy of target
    std::string target = forwardResponse.target.text;
    Response backwardResponse = translateForResponse_(service_, models.back(), std::move(target), responseOptions);

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
    const Response response = translateForResponse_(service_, model, std::move(source), responseOptions);

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
    const Response response = translateForResponse_(service_, model, std::move(source), responseOptions);

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
    Response firstResponse = translateForResponse_(service_, model, source, responseOptions);

    auto statsFirstRun = service_.cacheStats();
    LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
    ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

    // Round 2; There should be cache hits
    Response secondResponse = translateForResponse_(service_, model, source, responseOptions);

    auto statsSecondRun = service_.cacheStats();
    LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
    ABORT_IF(statsSecondRun.hits <= 0, "At least one hit expected, none found.");
    ABORT_IF(statsSecondRun.hits != statsFirstRun.misses,
             "Mismatch in expected hits. This test is supposed to check if all previous misses are hit in second run. "
             "Ensure you give an input file and cache-size caps within reasonable limits.");

    ABORT_IF(firstResponse.target.text != secondResponse.target.text,
             "Recompiled string provided different output when operated with cache. On the same hardware while using "
             "same path, this is expected to be same.");

    std::cout << firstResponse.target.text;
  }

  void benchmarkCacheEditWorkflow(Ptr<TranslationModel> model) {
    std::cout << "Starting cache-warmup" << std::endl;
    Response response;

    {
      ResponseOptions responseOptions;
      std::string input = readFromStdin();

      // Running this once lets the tokenizer work it's magic in response.source (annotation).
      response = translateForResponse_(service_, model, std::move(input), responseOptions);
    }

    std::cout << "Completed first round of translations!" << std::endl;

    ResponseOptions responseOptions;
    // Hyperparameters
    std::mt19937 generator;
    generator.seed(42);

    enum class Action { ERROR_THEN_CORRECT_STOP, CORRECT_STOP, TYPE_THROUGH };
    std::discrete_distribution<> actionSampler({0.05, 0.15, 0.8});

    std::vector<size_t> counts(/*numActions=*/3, /*init-value=*/0);

    // A simple state machine which advances each step and ends after a finite number of steps. The choice of a bunch of
    // mistakes are probabilistic.
    size_t previousWordEnd = 0;
    const std::string &input = response.source.text;
    std::string buffer;
    Response editResponse;
    std::cout << "Number of sentences: " << response.source.numSentences() << std::endl;

    marian::timer::Timer taskTimer;
    for (size_t s = 0; s < response.source.numSentences(); s++) {
      for (size_t w = 0; w < response.source.numWords(s); w++) {
        ByteRange currentWord = response.source.wordAsByteRange(s, w);
        int index = actionSampler(generator);
        ++counts[index];

        Action action = static_cast<Action>(index);
        switch (action) {
          case Action::ERROR_THEN_CORRECT_STOP: {
            // Error once
            buffer = input.substr(0, previousWordEnd) + " 0xdeadbeef" /* highly unlikely error token */;
            editResponse = translateForResponse_(service_, model, std::move(buffer), responseOptions);

            // Backspace a token
            buffer = input.substr(0, previousWordEnd);
            editResponse = translateForResponse_(service_, model, std::move(buffer), responseOptions);

            // Correct
            buffer = input.substr(0, currentWord.end);
            editResponse = translateForResponse_(service_, model, std::move(buffer), responseOptions);
            break;
          }

          case Action::CORRECT_STOP: {
            buffer = input.substr(0, currentWord.end);
            editResponse = translateForResponse_(service_, model, std::move(buffer), responseOptions);
            break;
          }

          case Action::TYPE_THROUGH: {
            break;
          }

          default: {
            ABORT("Unknown action");
            break;
          }
        }
        previousWordEnd = currentWord.end;
      }
    }

    auto cacheStats = service_.cacheStats();
    std::cout << "Hits / Misses = " << cacheStats.hits << "/ " << cacheStats.misses << std::endl;
    std::cout << "Action samples: ";
    for (size_t index = 0; index < counts.size(); index++) {
      std::cout << "{" << index << ":" << counts[index] << "} ";
    }
    std::cout << std::endl;
    LOG(info, "Total time: {:.5f}s wall", taskTimer.elapsed());
  }

  void wngt20IncrementalDecodingForCache(Ptr<TranslationModel> model) {
    // In this particular benchmark-run, we don't care for speed. We run through WNGT 1M sentences, all hopefully
    // unique. Analyzing cache usage every 1K sentences.
    marian::timer::Timer decoderTimer;
    ResponseOptions responseOptions;
    // Read a large input text blob from stdin

    std::cout << "[";

    auto processDelta = [this, &model, &responseOptions](size_t lineBegin, size_t lineEnd, std::string &&buffer) {
      // Once we have the interval lines, send it for translation.
      Response response = translateForResponse_(service_, model, std::move(buffer), responseOptions);
      auto cacheStats = service_.cacheStats();

      // The following prints a JSON, not great, but enough to be consumed later in python.
      if (lineBegin != 0) {
        std::cout << "," << '\n';
      }
      std::cout << "{\n";
      std::cout << "\"lines\" : " << lineEnd << ",\n ";
      std::cout << "\"hits\" : " << cacheStats.hits << ",\n ";
      std::cout << "\"misses\" : " << cacheStats.misses << ",\n ";
      std::cout << "\"evictedRecords\": " << cacheStats.evictedRecords << ",\n";
      std::cout << "\"activeRecords\": " << cacheStats.activeRecords << ",\n";
      std::cout << "\"totalSize\": " << cacheStats.totalSize << "\n";
      std::cout << "}\n";
    };

    constexpr size_t interval = 1000;
    bool first = true;
    std::string buffer, line;
    size_t lineId;
    for (lineId = 0; std::getline(std::cin, line); lineId++) {
      buffer += line;
      buffer += "\n";

      if ((lineId + 1) % interval == 0) {
        // [lineBegin, lineEnd) representing the range.
        processDelta(/*lineBegin=*/(lineId + 1) - interval, /*lineEnd=*/lineId + 1, std::move(buffer));
        buffer.clear();
      }
    }

    if (!buffer.empty()) {
      processDelta(/*lineBegin=*/(lineId + 1) - interval, /*lineEnd=*/lineId + 1, std::move(buffer));
    }

    std::cout << "]";
    std::cout << std::endl;

    // LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  }
};

}  // namespace marian::bergamot
