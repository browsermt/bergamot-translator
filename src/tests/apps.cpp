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

  // Wait on future until Response is complete
  std::future<Response> responseFuture = service.translate(std::move(input), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();
  return response;
}

void qualityScores(Ptr<Options> options) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;

  Response response = translateFromStdin(options, responseOptions);
  for (int sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    auto &quality = response.qualityScores[sentenceIdx];
    std::cout << ((sentenceIdx == 0) ? "" : "\n") << quality.sequence << '\n';
    for (int wordIdx = 0; wordIdx < quality.word.size(); wordIdx++) {
      std::cout << ((wordIdx == 0) ? "" : " ");
      std::cout << quality.word[wordIdx];
    }
    std::cout << '\n';
  }
}

void alignmentAggregatedToSource(Ptr<Options> options, bool numeric) {
  ResponseOptions responseOptions;
  responseOptions.alignment = true;
  responseOptions.alignmentThreshold = 0.2f;
  Response response = translateFromStdin(options, responseOptions);

  for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << (sentenceIdx == 0 ? "" : "\n");

    // We are aggregating at source, which does not depend on matrix-multiplications and printing only target so we can
    // do BLEU based stuff on the text.
    //
    typedef std::pair<size_t, float> Point;

    std::vector<std::vector<Point>> aggregate(response.source.numWords(sentenceIdx));
    auto &alignments = response.alignments[sentenceIdx];
    for (auto &p : alignments) {
      aggregate[p.src].emplace_back(p.tgt, p.prob);
    }

    for (size_t sourceIdx = 0; sourceIdx < aggregate.size(); sourceIdx++) {
      // Sort in order of target tokens.
      auto cmp = [](const Point &p, const Point &q) { return p.first < q.first; };
      std::sort(aggregate[sourceIdx].begin(), aggregate[sourceIdx].end(), cmp);

      if (!numeric) {
        std::cout << response.source.word(sentenceIdx, sourceIdx) << ": ";
      }

      for (size_t j = 0; j < aggregate[sourceIdx].size(); j++) {
        if (numeric) {
          float alignmentScore = aggregate[sourceIdx][j].second;
          std::cout << (j == 0 ? "" : " ");
          std::cout << alignmentScore;
        } else {
          std::cout << " ";
          size_t targetIdx = aggregate[sourceIdx][j].first;
          std::cout << response.target.word(sentenceIdx, targetIdx);
        }
      }
      std::cout << '\n';
    }
  }
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

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
