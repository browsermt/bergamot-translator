#pragma once
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

// marian::bergamot:: makes life easier, won't need to prefix it everywhere and these classes plenty use constructs.

/// Interface for command-line applications. All applications are expected to use the Options based parsing until
/// someone builds a suitable non-marian based yet complete replacement.

class CLIAppInterface {
 public:
  virtual void execute(Ptr<Options> options);
};

/// Previously bergamot-translator-app. Expected to be maintained consistent with how the browser (mozilla, WASM)
/// demands its API and tests be intact.

class WASMApp : public CLIAppInterface {
 public:
  void execute(Ptr<Options> options) {
    std::string config = options->asYamlString();

    // Route the config string to construct marian model through TranslationModel
    Service model(config);

    ResponseOptions responseOptions;
    std::vector<std::string> texts;

    for (std::string line; std::getline(std::cin, line);) {
      texts.emplace_back(line);
    }

    auto results = model.translateMultiple(std::move(texts), responseOptions);

    for (auto &result : results) {
      std::cout << result.getTranslatedText() << std::endl;
    }
  }
};

/// Application used to benchmark with marian-decoder from time-to time. The implementation in this repository follows a
/// different route (request -> response based instead of a file with EOS based) and routinely needs to be checked that
/// the speeds while operating similar to marian-decoder are not affected during the course of development. Expected to
/// be compatible with Translator[1] and marian-decoder[2].
/// [1] https://github.com/marian-nmt/marian-dev/blob/master/src/translator/translator.h
/// [2] https://github.com/marian-nmt/marian/blob/master/src/command/marian_decoder.cpp

class MarianDecoder : public CLIAppInterface {
 public:
  void execute(Ptr<Options> options) {
    marian::timer::Timer decoderTimer;
    Service service(options);
    // Read a large input text blob from stdin
    std::ostringstream std_input;
    std_input << std::cin.rdbuf();
    std::string input = std_input.str();

    // Wait on future until Response is complete
    std::future<Response> responseFuture = service.translate(std::move(input));
    responseFuture.wait();
    const Response &response = responseFuture.get();

    for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
      std::cout << response.target.sentence(sentenceIdx) << "\n";
    }
    LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  }
};

/// The translation-service here provides more features than conventional marian-decoder. A primary one being able to
/// track tokens in (unnormalized) source-text and translated text using ByteRanges. In addition, alignments and
/// quality-scores are exported in the Response for consumption downstream.
class ServiceCLI : public CLIAppInterface {
 public:
  void execute(Ptr<Options> options) {
    // Prepare memories for bytearrays (including model, shortlist and vocabs)
    MemoryBundle memoryBundle;

    if (options->get<bool>("bytearray")) {
      // Load legit values into bytearrays.
      memoryBundle = getMemoryBundleFromConfig(options);
    }

    Service service(options, std::move(memoryBundle));

    // Read a large input text blob from stdin
    std::ostringstream std_input;
    std_input << std::cin.rdbuf();
    std::string input = std_input.str();

    ResponseOptions responseOptions;
    responseOptions.qualityScores = true;
    responseOptions.alignment = true;
    responseOptions.alignmentThreshold = 0.2f;

    // Wait on future until Response is complete
    std::future<Response> responseFuture = service.translate(std::move(input), responseOptions);
    responseFuture.wait();
    Response response = responseFuture.get();

    std::cout << "[original]: " << response.source.text << '\n';
    std::cout << "[translated]: " << response.target.text << '\n';
    for (int sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
      std::cout << " [src Sentence]: " << response.source.sentence(sentenceIdx) << '\n';
      std::cout << " [tgt Sentence]: " << response.target.sentence(sentenceIdx) << '\n';
      std::cout << "Alignments" << '\n';
      typedef std::pair<size_t, float> Point;

      // Initialize a point vector.
      std::vector<std::vector<Point>> aggregate(response.source.numWords(sentenceIdx));

      // Handle alignments
      auto &alignments = response.alignments[sentenceIdx];
      for (auto &p : alignments) {
        aggregate[p.src].emplace_back(p.tgt, p.prob);
      }

      for (size_t src = 0; src < aggregate.size(); src++) {
        std::cout << response.source.word(sentenceIdx, src) << ": ";
        for (auto &p : aggregate[src]) {
          std::cout << response.target.word(sentenceIdx, p.first) << "(" << p.second << ") ";
        }
        std::cout << '\n';
      }

      // Handle quality.
      auto &quality = response.qualityScores[sentenceIdx];
      std::cout << "Quality: whole(" << quality.sequence << "), tokens below:" << '\n';
      size_t wordIdx = 0;
      bool first = true;
      for (auto &p : quality.word) {
        if (first) {
          first = false;
        } else {
          std::cout << " ";
        }
        std::cout << response.target.word(sentenceIdx, wordIdx) << "(" << p << ")";
        wordIdx++;
      }
      std::cout << '\n';
    }
    std::cout << "--------------------------\n";
    std::cout << '\n';
  }
};

/// Template based duplication to call the respective classes based on mode string at run-time.
template <class CLIAppType>
void executeApp(Ptr<Options> options) {
  CLIAppType app;
  app.execute(options);
}

void execute(const std::string &mode, Ptr<Options> options) {
  if (mode == "wasm") {
    executeApp<WASMApp>(options);
  } else if (mode == "service-cli") {
    executeApp<ServiceCLI>(options);
  } else if (mode == "decoder") {
    executeApp<MarianDecoder>(options);
  } else {
    ABORT("Unknown --mode {}. Use one of {wasm,service-cli,decoder}", mode);
  }
}

}  // namespace bergamot
}  // namespace marian
