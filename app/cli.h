#ifndef BERGAMOT_APP_CLI_H
#define BERGAMOT_APP_CLI_H
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

// marian::bergamot:: makes life easier, won't need to prefix it everywhere and these classes plenty use constructs.

namespace app {

/// Previously bergamot-translator-app. Provides a command-line app on native which executes the code-path used by Web
/// Assembly. Expected to be maintained consistent with how the browser (Mozilla through WebAssembly) dictates its API
/// and tests be intact. Also used in [bergamot-evaluation](https://github.com/mozilla/bergamot-evaluation).
///
/// Usage example:
/// [brt/tests/basic/test_bergamot_translator_app_intgemm_8bit.cpu-threads.0.sh](https://github.com/browsermt/bergamot-translator-tests/blob/main/tests/basic/test_bergamot_translator_app_intgemm_8bit.cpu-threads.0.sh)
///
/// * Input : read from stdin as sentences as lines of text.
/// * Output: written to stdout as translations for the sentences supplied in corresponding lines
///
/// @param [options]: Options to translate passed down to marian through Options.
void wasm(const CLIConfig &config) {
  // Here, we take the command-line interface which is uniform across all apps. This is parsed into Ptr<Options> by
  // marian. However, mozilla does not allow a Ptr<Options> constructor and demands an std::string constructor since
  // std::string isn't marian internal unlike Ptr<Options>. Since this std::string path needs to be tested for mozilla
  // and since this class/CLI is intended at testing mozilla's path, we go from:
  //
  // cmdline -> Ptr<Options> -> std::string -> TranslationModel(std::string)
  //
  // Overkill, yes.

  const std::string &modelConfigPath = config.modelConfigPaths.front();

  Ptr<Options> options = parseOptionsFromFilePath(modelConfigPath);
  MemoryBundle memoryBundle = getMemoryBundleFromConfig(options);

  Ptr<TranslationModel> translationModel =
      New<TranslationModel>(options->asYamlString(), /*replicas=*/1, std::move(memoryBundle));

  BlockingService service;

  ResponseOptions responseOptions;
  std::vector<std::string> texts;

  // Hide the translateMultiple operation
  for (std::string line; std::getline(std::cin, line);) {
    texts.emplace_back(line);
  }

  auto results = service.translateMultiple(translationModel, std::move(texts), responseOptions);

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }
}

/// Application used to benchmark with marian-decoder from time-to-time. The implementation in this repository follows a
/// different route than marian-decoder  and routinely needs to be checked that the speeds while operating similar to
/// marian-decoder are not affected during the course of development.
///
/// Example usage:
/// [brt/speed-tests/test_wngt20_perf.sh](https://github.com/browsermt/bergamot-translator-tests/blob/main/speed-tests/test_wngt20_perf.sh).
///
/// Expected to be compatible with Translator[1] and marian-decoder[2].
///
/// - [1]
/// [marian-dev/../src/translator/translator.h](https://github.com/marian-nmt/marian-dev/blob/master/src/translator/translator.h)
/// - [2]
/// [marian-dev/../src/command/marian_decoder.cpp](https://github.com/marian-nmt/marian/blob/master/src/command/marian_decoder.cpp)
///
/// * Input: stdin, lines containing sentences, same as marian-decoder.
/// * Output: to stdout, translations of the sentences supplied via stdin in corresponding lines
///
/// @param [in] options: constructed from command-line supplied arguments
void decoder(const CLIConfig &config) {
  marian::timer::Timer decoderTimer;
  AsyncService service(config.numWorkers);
  size_t numWorkers = config.numWorkers;
  auto options = parseOptionsFromFilePath(config.modelConfigPaths.front());
  MemoryBundle memoryBundle;
  Ptr<TranslationModel> translationModel =
      New<TranslationModel>(options, /*replicas=*/numWorkers, std::move(memoryBundle));
  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();

  // Wait on future until Response is complete
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();
  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

  service.translate(translationModel, std::move(input), std::move(callback));
  responseFuture.wait();
  const Response &response = responseFuture.get();

  for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << response.target.sentence(sentenceIdx) << "\n";
  }
  LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
}

/// Command line interface to the test the features being developed as part of bergamot C++ library on native platform.
///
/// Usage example:
/// [brt/tests/basic/test_service-cli_intgemm_8bit.cpu-threads.4.sh](https://github.com/browsermt/bergamot-translator-tests/blob/main/tests/basic/test_service-cli_intgemm_8bit.cpu-threads.4.sh)
///
/// * Input: reads from stdin, blob of text, read as a whole ; sentence-splitting etc handled internally.
/// * Output: to stdout, translation of the source text faithful to source structure.
///
/// @param [in] options: options to build translator
void native(const CLIConfig &config) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;

  auto options = parseOptionsFromFilePath(config.modelConfigPaths.front());

  if (config.byteArray) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  size_t numWorkers = config.numWorkers;
  Ptr<TranslationModel> translationModel =
      New<TranslationModel>(options, /*replicas=*/numWorkers, std::move(memoryBundle));

  AsyncService service(config.numWorkers);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();

  ResponseOptions responseOptions;

  // Wait on future until Response is complete
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();
  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

  service.translate(translationModel, std::move(input), std::move(callback), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();

  std::cout << response.target.text;
}

}  // namespace app

}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_APP_CLI_H
