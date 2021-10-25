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

void pivotTranslate(AsyncService &service, std::vector<Ptr<TranslationModel>> &models) {
  // We expect a source -> pivot; pivot -> source model to get source -> source and build this test using accuracy of
  // matches.
  ABORT_IF(models.size() != 2, "Forward and backward test needs two models.");
  ResponseOptions responseOptions;
  responseOptions.alignment = true;
  std::string source = readFromStdin();
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.pivot(models.front(), models.back(), std::move(source), callback, responseOptions);

  responseFuture.wait();

  const float EPS = 1e-5;
  size_t totalOutcomes = 0;
  size_t favourableOutcomes = 0;

  Response response = responseFuture.get();
  for (size_t sentenceId = 0; sentenceId < response.source.numSentences(); sentenceId++) {
    std::cout << "> " << response.source.sentence(sentenceId) << "\n";
    std::cout << "< " << response.target.sentence(sentenceId) << "\n\n";

    // Assert what we have is a probability distribution over source-tokens given a target token.
    for (size_t t = 0; t < response.alignments[sentenceId].size(); t++) {
      float sum = 0.0f;
      for (size_t s = 0; s < response.alignments[sentenceId][t].size(); s++) {
        sum += response.alignments[sentenceId][t][s];
      }

      std::cerr << fmt::format("Sum @ (target-token = {}, sentence = {}) = {}", t, sentenceId, sum) << std::endl;
      ABORT_IF((std::abs(sum - 1.0f) > EPS), "Not a probability distribution, something's going wrong");
    }

    // For each target token, find argmax s, i.e find argmax p(s | t), max p(s | t)
    for (size_t t = 0; t < response.alignments[sentenceId].size(); t++) {
      bool valid = false;
      float maxV = 0.0f;
      auto argmaxV = std::make_pair(-1, -1);
      for (size_t s = 0; s < response.alignments[sentenceId][t].size(); s++) {
        auto v = response.alignments[sentenceId][t][s];
        if (v > maxV) {
          maxV = v;
          argmaxV = std::make_pair(t, s);
        }
      }

      auto sourceToken = response.source.word(sentenceId, argmaxV.second);
      auto targetToken = response.target.word(sentenceId, argmaxV.first);
      if (sourceToken == targetToken) {
        favourableOutcomes += 1;
      }

      std::cerr << sourceToken << " " << targetToken << " " << maxV << std::endl;

      totalOutcomes += 1;
    }

    // Assert each alignment over target is a valid probability distribution.
  }

  // Measure accuracy of word match.
  float accuracy = static_cast<float>(favourableOutcomes) / static_cast<float>(totalOutcomes);

  // This is arbitrary value chosen by @jerinphilip, but should be enough to check if things fail.
  // This value is calibrated on bergamot input in BRT. All this is supposed to do is let the developers know if
  // something's largely amiss to the point alignments are not working.
  ABORT_IF(accuracy < 0.70, "Accuracy {} not enough. Please check if something's off.", accuracy * 100);

  std::cout << response.source.text;
  std::cout << response.target.text;
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

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
