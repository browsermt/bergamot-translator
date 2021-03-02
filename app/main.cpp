/*
 * main.cpp
 *
 * An example application to demonstrate the use of Bergamot translator.
 *
 */

#include <iostream>

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
    for (int idx = 0; idx < result.size(); idx++) {
      std::cout << " [src Sentence]: " << result.source_sentence(idx)
                << std::endl;
      std::cout << " [tgt Sentence]: " << result.translated_sentence(idx)
                << std::endl;
      std::cout << "Alignments" << std::endl;
      auto &alignments = result.alignment(idx);
      for (auto &p : alignments) {
        std::cout << result.source_word(idx, p.src) << " "
                  << result.translation_word(idx, p.tgt) << ": " << p.prob
                  << std::endl;
      }
    }
    std::cout << std::endl;
  }

  return 0;
}
