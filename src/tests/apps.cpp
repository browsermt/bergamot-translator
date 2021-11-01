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
                              ResponseOptions responseOptions, TagPositions &&tagPositionsSource = {}) {
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(model, std::move(source), callback, responseOptions, std::move(tagPositionsSource));

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

void translationCache(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;

  // Read a large input text blob from stdin
  const std::string source = readFromStdin();

  // Round 1
  std::string buffer = source;
  Response firstResponse = translateForResponse(service, model, std::move(buffer), responseOptions);

  auto statsFirstRun = service.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
  ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

  // Round 2; There should be cache hits
  buffer = source;
  Response secondResponse = translateForResponse(service, model, std::move(buffer), responseOptions);

  auto statsSecondRun = service.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
  ABORT_IF(statsSecondRun.hits <= 0, "At least one hit expected, none found.");
  if (statsSecondRun.hits != statsFirstRun.misses) {
    std::cerr << "Mismatch in expected hits (Hits, Misses = " << statsSecondRun.hits << ", " << statsSecondRun.misses
              << "). This can happen due to random eviction." << std::endl;
  }

  ABORT_IF(firstResponse.target.text != secondResponse.target.text,
           "Recompiled string provided different output when operated with cache. On the same hardware while using "
           "same path, this is expected to be same.");

  std::cout << firstResponse.target.text;
}

void tagTranslation(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.alignment = true;
  std::string source = readFromStdin();

  std::mt19937 g;  // seed the generator
  g.seed(42);

  // This function is local here, so we will use std::function to gain some recursion in lambda.
  std::function<void(std::vector<ByteRange> &, int, int, int)> placeTags;
  placeTags = [&g, &placeTags](std::vector<ByteRange> &tags, int l, int r, int remaining) {
    if (remaining == 0) return;

    if (remaining == 1) {
      tags.push_back({static_cast<size_t>(l), static_cast<size_t>(r)});
      return;
    }

    // Choose a random split-point in [l, r).
    std::uniform_int_distribution<> dist(l, r);
    int splitPoint = dist(g);

    std::uniform_int_distribution<> pdist(1, remaining - 1);
    int k = pdist(g);

    tags.push_back({static_cast<size_t>(l), static_cast<size_t>(r)});
    placeTags(tags, l, splitPoint, remaining - 1 - k);
    placeTags(tags, splitPoint, r, k);
  };

  std::vector<ByteRange> tagPosSourceCharLevel;
  tagPosSourceCharLevel.push_back({0, source.size()});
  placeTags(tagPosSourceCharLevel, 0, source.size() - 1, /*nodes=*/5);

  std::cout << source;
  std::cout << "size: " << source.size() << std::endl;
  bool first = true;
  std::cout << "Source tag - traversal { ";
  for (auto &r : tagPosSourceCharLevel) {
    if (!first) {
      std::cout << ", ";
    } else {
      first = false;
    }
    std::cout << fmt::format("[{}, {})", r.begin, r.end);
  }
  std::cout << "} " << std::endl;

  Response response =
      translateForResponse(service, model, std::move(source), responseOptions, std::move(tagPosSourceCharLevel));

  std::cout << "Translated character-level ByteRange array:" << std::endl;
  for (ByteRange br : response.target.tagPositions) {
    std::cout << "[" << br.begin << "," << br.end << ")";
    for (size_t pos = br.begin; pos < br.end; pos++) {
      std::cout << response.target.text[pos];
    }
    std::cout << std::endl;
  }
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
