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

template <class Service>
class TestSuite {
 private:
  TranslateForResponse<Service> translateForResponse_;
  Service &service_;

 public:
  TestSuite(Service &service) : service_(service), translateForResponse_() {}

  void run(const OpMode opMode, std::vector<Ptr<TranslationModel>> &models) {
    switch (opMode) {
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

      default:
        ABORT("Incompatible op-mode. Choose one of the test modes.");
        break;
    }
  }

 private:
  void benchmarkDecoder(Ptr<TranslationModel> &model) {
    marian::timer::Timer decoderTimer;
    std::string source = readFromStdin();

    ResponseOptions responseOptions;
    Response response = translateForResponse_(service_, model, std::move(source), responseOptions);

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
};

}  // namespace marian::bergamot
