#include "apps.h"

namespace marian {
namespace bergamot {

namespace {

std::string readFromStdin() {
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();
  return input;
}

// Utility function, common for all testapps.
Response translateForResponse(AsyncService &service, Ptr<TranslationModel> model, std::string &&source,
                              ResponseOptions responseOptions) {
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(model, std::move(source), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
  return response;
}

}  // namespace

namespace testapp {

void annotatedTextWords(AsyncService &service, Ptr<TranslationModel> model, bool sourceSide) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = translateForResponse(service, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotatedTextSentences(AsyncService &service, Ptr<TranslationModel> model, bool sourceSide) {
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response response = translateForResponse(service, model, std::move(source), responseOptions);
  AnnotatedText &annotatedText = sourceSide ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

void forwardAndBackward(AsyncService &service, std::vector<Ptr<TranslationModel>> &models) {
  ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
  ResponseOptions responseOptions;
  std::string source = readFromStdin();
  Response forwardResponse = translateForResponse(service, models.front(), std::move(source), responseOptions);

  // Make a copy of target
  std::string target = forwardResponse.target.text;
  Response backwardResponse = translateForResponse(service, models.back(), std::move(target), responseOptions);

  // Print both onto the command-line
  std::cout << forwardResponse.source.text;
  std::cout << "----------------\n";
  std::cout << forwardResponse.target.text;
  std::cout << "----------------\n";
  std::cout << backwardResponse.target.text;
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
