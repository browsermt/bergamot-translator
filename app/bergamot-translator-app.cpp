/*
 * main.cpp
 *
 * An application which accepts line separated texts in stdin and returns
 * translated ones in stdout. It is convenient for batch processing and can be
 * used with tools like SacreBLEU.
 *
 */

#include <iostream>
#include <string>

#include "translator/parser.h"
#include "translator/service.h"

int main(int argc, char **argv) {
#ifdef WASM_COMPATIBLE_SOURCE

  // Create a configParser and load command line parameters into a YAML config
  // string.
  auto configParser = marian::bergamot::createConfigParser();
  auto options = configParser.parseOptions(argc, argv, true);
  std::string config = options->asYamlString();

  // Route the config string to construct marian model through TranslationModel
  marian::bergamot::Service model(config);

  marian::bergamot::ResponseOptions responseOptions;
  std::vector<std::string> texts;

  for (std::string line; std::getline(std::cin, line);) {
    texts.emplace_back(line);
  }

  auto results = model.translateMultiple(std::move(texts), responseOptions);

  for (auto &result : results) {
    std::cout << result.getTranslatedText() << std::endl;
  }
#endif

  return 0;
}
