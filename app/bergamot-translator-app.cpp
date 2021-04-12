/*
 * main.cpp
 *
 * An application which accepts line separated texts in stdin and returns translated ones in stdout.
 * It is convenient for batch processing and can be used with tools like SacreBLEU.
 *
 */

#include <iostream>
#include <string>

#include "TranslationModel.h"
#include "translator/parser.h"

int main(int argc, char **argv) {

  // Create a configParser and load command line parameters into a YAML config
  // string.
  auto configParser = marian::bergamot::createConfigParser();
  auto options = configParser.parseOptions(argc, argv, true);
  std::string config = options->asYamlString();

  // Route the config string to construct marian model through TranslationModel
  TranslationModel model(config);

  TranslationRequest translationRequest;
  std::vector<std::string> texts;

  for (std::string line; std::getline(std::cin, line);) {
        texts.emplace_back(line);
  }

  auto results = model.translate(std::move(texts), translationRequest);

  // Resolve the future and get the actual result
  //std::vector<TranslationResult> results = futureResults.get();

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }

  return 0;
}
