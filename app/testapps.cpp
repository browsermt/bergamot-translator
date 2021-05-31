#include "testapps.h"

namespace marian {
namespace bergamot {
namespace testapp {

// Utility function, common for all testapps.
Response translate_from_stdin(Ptr<Options> options, ResponseOptions responseOptions) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;

  if (options->get<bool>("bytearray")) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  Service service(options, std::move(memoryBundle));

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();

  // Wait on future until Response is complete
  std::future<Response> responseFuture = service.translate(std::move(input), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();
  return response;
}

void quality_scores(Ptr<Options> options) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;

  Response response = translate_from_stdin(options, responseOptions);
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

void alignment_aggregated_to_source(Ptr<Options> options, bool numeric) {
  ResponseOptions responseOptions;
  responseOptions.alignment = true;
  responseOptions.alignmentThreshold = 0.2f;
  Response response = translate_from_stdin(options, responseOptions);

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

void annotated_text_words(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translate_from_stdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotated_text_sentences(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translate_from_stdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

void legacy_service_cli(Ptr<Options> options) {
  ResponseOptions responseOptions;
  responseOptions.qualityScores = true;
  responseOptions.alignment = true;
  responseOptions.alignmentThreshold = 0.2f;
  Response response = translate_from_stdin(options, responseOptions);

  std::cout << "[original]: " << response.source.text << '\n';
  std::cout << "[translated]: " << response.target.text << '\n';
  for (int sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << " [src Sentence]: " << response.source.sentence(sentenceIdx) << '\n';
    std::cout << " [tgt Sentence]: " << response.target.sentence(sentenceIdx) << '\n';
    std::cout << "Alignments" << '\n';
    typedef std::pair<size_t, float> Point;

    // Initialize a point vector.
    std::vector<std::vector<Point>> aggregate(response.source.numWords(sentenceIdx));

    // Handle alignments
    auto &alignments = response.alignments[sentenceIdx];
    for (auto &p : alignments) {
      aggregate[p.src].emplace_back(p.tgt, p.prob);
    }

    for (size_t src = 0; src < aggregate.size(); src++) {
      std::cout << response.source.word(sentenceIdx, src) << ": ";
      for (auto &p : aggregate[src]) {
        std::cout << response.target.word(sentenceIdx, p.first) << "(" << p.second << ") ";
      }
      std::cout << '\n';
    }

    // Handle quality.

    auto &quality = response.qualityScores[sentenceIdx];
    std::cout << "Quality: whole(" << quality.sequence << "), tokens below:" << '\n';
    size_t wordIdx = 0;
    bool first = true;
    for (auto &p : quality.word) {
      if (first) {
        first = false;
      } else {
        std::cout << " ";
      }
      std::cout << response.target.word(sentenceIdx, wordIdx) << "(" << p << ")";
      wordIdx++;
    }
    std::cout << '\n';
  }
  std::cout << "--------------------------\n";
  std::cout << '\n';
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
