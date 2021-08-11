#include "apps.h"

namespace marian {
namespace bergamot {
namespace testapp {

// Utility function, common for all testapps.
Response translateFromStdin(AsyncService &service, const TranslationModel::Config &modelConfig,
                            ResponseOptions responseOptions) {
  Ptr<TranslationModel> translationModel = New<TranslationModel>(modelConfig, service.numWorkers());
  // Read a large input text blob from stdin
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();

  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(translationModel, std::move(input), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
  return response;
}

void annotatedTextWords(AsyncService &service, const TranslationModel::Config &modelConfig, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(service, modelConfig, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotatedTextSentences(AsyncService &service, const TranslationModel::Config &modelConfig, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(service, modelConfig, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
