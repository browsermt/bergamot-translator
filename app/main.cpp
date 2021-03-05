/*
 * main.cpp
 *
 * An example application to demonstrate the use of Bergamot translator.
 *
 */

#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "AbstractTranslationModel.h"
#include "TranslationRequest.h"
#include "TranslationResult.h"
#include "translator/parser.h"

int main(int argc, char **argv) {

  // Create a configParser and load command line parameters into a YAML config
  // string.
  auto configParser = marian::bergamot::createConfigParser();
  auto options = configParser.parseOptions(argc, argv, true);
  std::string config = options->asYamlString();

  // Route the config string to construct marian model through
  // AbstractTranslationModel
  std::shared_ptr<AbstractTranslationModel> model =
      AbstractTranslationModel::createInstance(config);

  TranslationRequest translationRequest;
  std::vector<std::string> texts;
  texts.emplace_back(
      "The Bergamot project will add and improve client-side machine "
      "translation in a web browser.  Unlike current cloud-based "
      "options, running directly on users’ machines empowers citizens to "
      "preserve their privacy and increases the uptake of language "
      "technologies in Europe in various sectors that require "
      "confidentiality.");
  texts.emplace_back(
      "Free software integrated with an open-source web "
      "browser, such as Mozilla Firefox, will enable bottom-up adoption "
      "by non-experts, resulting in cost savings for private and public "
      "sector users who would otherwise procure translation or operate "
      "monolingually.  Bergamot is a consortium coordinated by the "
      "University of Edinburgh with partners Charles University in "
      "Prague, the University of Sheffield, University of Tartu, and "
      "Mozilla.");

  auto results = model->translate(std::move(texts), translationRequest);

  // Resolve the future and get the actual result
  // std::vector<TranslationResult> results = futureResults.get();

  for (auto &result : results) {
    std::cout << "[original]: " << result.getOriginalText() << std::endl;
    std::cout << "[translated]: " << result.getTranslatedText() << std::endl;
    for (int sentenceIdx = 0; sentenceIdx < result.size(); sentenceIdx++) {
      std::cout << " [src Sentence]: " << result.source_sentence(sentenceIdx)
                << std::endl;
      std::cout << " [tgt Sentence]: "
                << result.translated_sentence(sentenceIdx) << std::endl;
      std::cout << "Alignments" << std::endl;
      typedef std::pair<size_t, float> Point;

      // Initialize a point vector.
      std::vector<std::vector<Point>> aggregate(
          result.numSourceWords(sentenceIdx));

      // Handle alignments
      auto &alignments = result.alignment(sentenceIdx);
      for (auto &p : alignments) {
        aggregate[p.src].emplace_back(p.tgt, p.prob);
      }

      for (size_t src = 0; src < aggregate.size(); src++) {
        std::cout << result.source_word(sentenceIdx, src) << ": ";
        for (auto &p : aggregate[src]) {
          std::cout << result.translation_word(sentenceIdx, p.first) << "("
                    << p.second << ") ";
        }
        std::cout << std::endl;
      }

      // Handle quality.
      auto &quality = result.quality(sentenceIdx);
      std::cout << "Quality: whole(" << quality.sequence
                << "), tokens below:" << std::endl;
      size_t wordIdx = 0;
      bool first = true;
      std::cout << "tokens" << quality.word.size() << " "
                << result.numTranslationWords(sentenceIdx) << std::endl;
      // assert(quality.word.size() ==
      //        result.numTranslationWords(sentenceIdx) + 1);
      for (auto &p : quality.word) {
        if (first) {
          first = false;
        } else {
          std::cout << " ";
        }
        // assert(wordIdx < result.numTranslationWords(sentenceIdx));
        std::cout << result.translation_word(sentenceIdx, wordIdx) << "(" << p
                  << ")";
        wordIdx++;
      }
      std::cout << std::endl;
    }
    std::cout << "--------------------------\n";
    std::cout << std::endl;
  }

  return 0;
}
