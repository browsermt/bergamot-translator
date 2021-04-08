/*
 * main.cpp
 *
 * An example application to demonstrate the use of Bergamot translator.
 *
 */

#include <iostream>

#include "TranslationModel.h"
#include "translator/parser.h"
#include "translator/byte_array_util.h"

int main(int argc, char **argv) {

  // Create a configParser and load command line parameters into a YAML config
  // string.
  auto configParser = marian::bergamot::createConfigParser();
  auto options = configParser.parseOptions(argc, argv, true);
  std::string config = options->asYamlString();

  // Route the config string to construct marian model through TranslationModel
  auto model = std::make_shared<TranslationModel>(config, marian::bergamot::getModelMemoryFromConfig(options));

  TranslationRequest translationRequest;
  std::vector<std::string> texts;

  for (std::string line; std::getline(std::cin, line);) {
        texts.emplace_back(line);
  }

  auto results = model->translate(std::move(texts), translationRequest);

  // Resolve the future and get the actual result
  //std::vector<TranslationResult> results = futureResults.get();

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }

  return 0;
}
