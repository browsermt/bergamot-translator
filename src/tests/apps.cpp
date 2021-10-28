#include "apps.h"

#include <translator/tag_processor.h>

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
>>>>>>> main
}

void generatorForTagTree(AsyncService &service, Ptr<TranslationModel> model) {
  ResponseOptions responseOptions;
  responseOptions.alignment = true;

  // Set up data: "A <i><b>republican</b> strategy</i> to counteract the re-election of Obama."
  std::string source = "A republican strategy to counteract the re-election of Obama.";
  source.erase(std::remove(source.begin(), source.end(), '\n'), source.end());
  std::vector<ByteRange> brvecCharLevel;
  brvecCharLevel.push_back(ByteRange{2, 21});
  brvecCharLevel.push_back(ByteRange{2, 12});

  const Response response = translateForResponse(service, model, std::move(source), responseOptions);
  ABORT_IF(response.source.numSentences() != 1, "Cross sentence byteranges are tricky at the moment.");

  std::cout << "std::string source = \"" << response.source.text << "\";\n";
  std::cout << "std::string target = \"" << response.target.text << "\";\n";

  std::cout << "source string length: " << response.source.text.size() << std::endl;
  std::vector<size_t> char2TokenTable(response.source.text.size(), 0);

  for (size_t sentenceId = 0; sentenceId < 1; sentenceId++) {
    // Step 0: create or update char-> token table
    for (size_t s = 0; s < response.source.numWords(sentenceId); s++) {
      auto sourceByteRange = response.source.wordAsByteRange(sentenceId, s);
      for (size_t cur = sourceByteRange.begin; cur < sourceByteRange.end; cur++) {
        char2TokenTable[cur] = s;
      }
    }

    // Step 1: convert char indices to token indices conversion
    std::vector<ByteRange> brvecOriginalTokenLevel;
    for (size_t tagIdx = 0; tagIdx < brvecCharLevel.size(); tagIdx++) {
      size_t charIdxBegin = brvecCharLevel[tagIdx].begin;
      size_t charIdxEnd = brvecCharLevel[tagIdx].end;
      brvecOriginalTokenLevel.push_back(ByteRange{char2TokenTable[charIdxBegin], char2TokenTable[charIdxEnd]});
    }

    // Step 2: build token-level tag tree
    TagTreeBuilder ttb(brvecOriginalTokenLevel);
    TagTree ttOriginalTokenLevel = ttb.getTagTree();
    // ttOriginalTokenLevel.print();
    std::cout << "Original token-level tag tree:" << std::endl;
    ttOriginalTokenLevel.print();

    // Step 3: call inside-outside algorithm
    auto &alignments = response.alignments[sentenceId];
    TagProcessor tp = TagProcessor(alignments, ttOriginalTokenLevel, response.source.numWords(sentenceId),
                                   response.target.numWords(sentenceId));
    int exitStatus = tp.traverseAndQuery();
    TagTree ttTranslatedTokenLevel = tp.getTargetRoot();
    std::cout << "Translated token-level tag tree:" << std::endl;
    ttTranslatedTokenLevel.print();

    // Step 4: flatten the token-level tag tree for the translated text to a token-level ByteRange vector
    std::vector<ByteRange> brvecTranslatedTokenLevel = ttTranslatedTokenLevel.flatten();

    // Step 5: convert the flattened token-level tag tree to the character level one
    std::vector<ByteRange> brvecTranslatedCharLevel;
    for (ByteRange br : brvecTranslatedTokenLevel) {
      size_t charBegin = response.target.wordAsByteRange(sentenceId, br.begin).begin;
      size_t charEnd = response.target.wordAsByteRange(sentenceId, br.end).begin;
      //      std::cout <<br.begin <<" " <<br.end <<std::endl;
      brvecTranslatedCharLevel.push_back(ByteRange{charBegin, charEnd});
    }
    std::cout << "Translated character-level ByteRange array:" << std::endl;
    for (ByteRange br : brvecTranslatedCharLevel) {
      std::cout << br.begin << " " << br.end;
      for (size_t pos = br.begin; pos < br.end; pos++) {
        std::cout << response.target.text[pos];
      }
      std::cout << std::endl;
    }
  }
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
