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

class CLIAppInterface {
 public:
  virtual void execute(Ptr<Options> options);
};

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
    std::abort();
  }
}

}  // namespace bergamot
}  // namespace marian
