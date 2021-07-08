#include "apps.h"

namespace marian {
namespace bergamot {
namespace testapp {

// Utility function, common for all testapps.
Response translateFromStdin(Ptr<Options> options, ResponseOptions responseOptions) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;

  if (options->get<bool>("bytearray")) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  Service service(options, std::move(memoryBundle));

  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();

  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(std::move(input), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
  return response;
}

void annotatedTextWords(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotatedTextSentences(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

void translationCache(Ptr<Options> options) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;
  ResponseOptions responseOptions;

  if (options->get<bool>("bytearray")) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  Service service(options, std::move(memoryBundle));

  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();

  auto translateForResponse = [&service, &responseOptions](std::string input) {
    std::promise<Response> responsePromise;
    std::future<Response> responseFuture = responsePromise.get_future();
    auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

    service.translate(std::move(input), callback, responseOptions);

    responseFuture.wait();
    Response response = responseFuture.get();
    return response;
  };

  Response response;
  auto &stats = service.cacheStats();

  // Round 1
  response = translateForResponse(input);
  LOG(info, "Cache Hits/Misses = {}/{}", stats.hits, stats.misses);
  ABORT_IF(stats.hits != 0, "Expecting no cache hits, but hits found.");

  // Round 2; There should be cache hits
  response = translateForResponse(input);
  LOG(info, "Cache Hits/Misses = {}/{}", stats.hits, stats.misses);
  ABORT_IF(stats.hits == 0, "No cache hits while expected non-zero");
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
