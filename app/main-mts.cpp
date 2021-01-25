#include <cstdlib>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/service.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  marian::bergamot::Service service(options);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();

  LOG(info, "IO complete Translating input");
  // Wait on future until TranslationResult is complete
  auto translation_result_future = service.translate(std::move(input));
  translation_result_future.wait();
  auto translation_result = translation_result_future.get();

  // Obtain sentencemappings and print them as Proof of Concept.
  for (auto &p : translation_result.getSentenceMappings()) {
    std::cout << "[src] " << p.first << "\n";
    std::cout << "[tgt] " << p.second << "\n";
  }

  // Stop Service.
  service.stop();
  return 0;
}
