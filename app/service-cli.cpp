#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  marian::bergamot::Service service(options);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();
  using marian::bergamot::Response;

  marian::bergamot::ResponseOptions responseOptions;
  responseOptions.qualityScores = true;
  responseOptions.alignment = true;
  responseOptions.alignmentThreshold = 0.2f;

  // Wait on future until Response is complete
  std::future<Response> responseFuture =
      service.translate(std::move(input), responseOptions);
  responseFuture.wait();
  Response response = responseFuture.get();

  std::cout << "[original]: " << response.source.text << '\n';
  std::cout << "[translated]: " << response.target.text << '\n';
  for (int sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
    std::cout << " [src Sentence]: " << response.source.sentence(sentenceIdx)
              << '\n';
    std::cout << " [tgt Sentence]: " << response.target.sentence(sentenceIdx)
              << '\n';
    std::cout << "Alignments" << '\n';
    typedef std::pair<size_t, float> Point;

    // Initialize a point vector.
    std::vector<std::vector<Point>> aggregate(
        response.source.numWords(sentenceIdx));

    // Handle alignments
    auto &alignments = response.alignments[sentenceIdx];
    for (auto &p : alignments) {
      aggregate[p.src].emplace_back(p.tgt, p.prob);
    }

    for (size_t src = 0; src < aggregate.size(); src++) {
      std::cout << response.source.word(sentenceIdx, src) << ": ";
      for (auto &p : aggregate[src]) {
        std::cout << response.target.word(sentenceIdx, p.first) << "("
                  << p.second << ") ";
      }
      std::cout << '\n';
    }

    // Handle quality.
    auto &quality = response.qualityScores[sentenceIdx];
    std::cout << "Quality: whole(" << quality.sequence
              << "), tokens below:" << '\n';
    size_t wordIdx = 0;
    bool first = true;
    for (auto &p : quality.word) {
      if (first) {
        first = false;
      } else {
        std::cout << " ";
      }
      std::cout << response.target.word(sentenceIdx, wordIdx) << "(" << p
                << ")";
      wordIdx++;
    }
    std::cout << '\n';
  }
  std::cout << "--------------------------\n";
  std::cout << '\n';

  return 0;
}
