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

void qualityEstimatorWords(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;
  std::string source = readFromStdin();
  const Response response = translateForResponse(service, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << "[SentenceBegin]\n";

    for (const auto &wordByteRange : sentenceQualityEstimate.wordByteRanges) {
      const string_view word(response.target.text.data() + wordByteRange.begin, wordByteRange.size());
      std::cout << word << "\n";
    }
    std::cout << "[SentenceEnd]\n\n";
  }
}

void qualityEstimatorScores(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;

  std::string source = readFromStdin();
  const Response response = translateForResponse(service, model, std::move(source), responseOptions);

  for (const auto &sentenceQualityEstimate : response.qualityScores) {
    std::cout << std::fixed << std::setprecision(3) << sentenceQualityEstimate.sentenceScore << "\n";

    for (const float &wordScore : sentenceQualityEstimate.wordScores) {
      std::cout << std::fixed << std::setprecision(3) << wordScore << "\n";
    }
    std::cout << "\n";
  }
}

void generatorForTagTree(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.alignment = true;

  std::string source = readFromStdin();
  source.erase(std::remove(source.begin(), source.end(), '\n'), source.end());

  const Response response = translateForResponse(service, model, std::move(source), responseOptions);

  ABORT_IF(response.source.numSentences() != 1, "Cross sentence byteranges are tricky at the moment.");

  std::cout << "std::string source = \"" << response.source.text << "\";\n";
  std::cout << "std::string target = \"" << response.target.text << "\";\n";

  for (size_t sentenceId = 0; sentenceId < 1; sentenceId++) {
    std::cout << "std::vector<ByteRange> sourceTokens =  {";
    for (size_t s = 0; s < response.source.numWords(sentenceId); s++) {
      if (s != 0) std::cout << ", ";
      auto sourceByteRange = response.source.wordAsByteRange(sentenceId, s);
      std::cout << "/*" << response.source.word(sentenceId, s) << "*/ {" << sourceByteRange.begin << ", "
                << sourceByteRange.end << " }";
    }

    std::cout << "};\n";

    std::cout << "std::vector<ByteRange> targetTokens =  {";
    for (size_t t = 0; t < response.target.numWords(sentenceId); t++) {
      if (t != 0) std::cout << ", ";
      auto targetByteRange = response.target.wordAsByteRange(sentenceId, t);
      std::cout << "/*" << response.target.word(sentenceId, t) << "*/ {" << targetByteRange.begin << ", "
                << targetByteRange.end << " }";
    }

    std::cout << "};\n";

    // Print alignments
    auto &alignments = response.alignments[sentenceId];
    std::cout << "marian::data::SoftAlignment> alignments = {\n";
    for (size_t t = 0; t < alignments.size(); t++) {
      if (t != 0) std::cout << ", ";
      std::cout << "{ ";
      for (size_t s = 0; s < alignments[t].size(); s++) {
        if (s != 0) std::cout << ", ";
        std::cout << alignments[t][s];
      }
      std::cout << "}\n";
    }

    std::cout << "};\n";
  }
}  // namespace testapp

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
