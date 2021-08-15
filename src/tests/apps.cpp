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

void qualityEstimatorScores(const Ptr<Options> &options) {
  ResponseOptions responseOptions;
  const Response response = translateFromStdin(options, responseOptions);

  for (size_t s = 0; s < response.target.numSentences(); ++s) {
    std::cout << "[src Sentence]:" << response.source.sentence(s) << "\n";
    std::cout << "[tgt Sentence]:" << response.target.sentence(s) << "\n";

    if (response.qualityScores.size() <= s) {
      continue;
    }

    const auto &wordsQualityEstimate = response.qualityScores[s];

    std::cout << "[score Sentence]:" << wordsQualityEstimate.sentenceScore << "\n";
    std::cout << "[words Scores]:";

    for (size_t i = 0; i < wordsQualityEstimate.wordQualityScores.size(); ++i) {
      const ByteRange wordByteRange = wordsQualityEstimate.wordByteRanges[i];
      const float wordScore = wordsQualityEstimate.wordQualityScores[i];
      const string_view word(response.target.text.data() + wordByteRange.begin, wordByteRange.size());

      if (i != 0) {
        std::cout << " ";
      }

      std::cout << word << "(" << std::fixed << std::setprecision( 3 ) << wordScore << ")";
    }
    std::cout << "\n\n";
  }
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
