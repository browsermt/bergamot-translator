#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/service.h"
#include "translator/translation_result.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  marian::bergamot::Service service(options);

  // Read a large input text blob from stdin
  std::ostringstream std_input;
  std_input << std::cin.rdbuf();
  std::string input = std_input.str();
  using marian::bergamot::TranslationResult;

  // Wait on future until TranslationResult is complete
  std::future<TranslationResult> translation_result_future =
      service.translate(std::move(input));
  translation_result_future.wait();
  const TranslationResult &translation_result = translation_result_future.get();

  std::cout << "service-cli [Source text]: ";
  std::cout << translation_result.getOriginalText() << std::endl;

  std::cout << "service-cli [Translated text]: ";
  std::cout << translation_result.getTranslatedText() << std::endl;

  // Obtain sentenceMappings and print them as Proof of Concept.
  const TranslationResult::SentenceMappings &sentenceMappings =
      translation_result.getSentenceMappings();
  for (auto &p : sentenceMappings) {
    std::cout << "service-cli [src] " << p.first << "\n";
    std::cout << "service-cli [tgt] " << p.second << "\n";
  }

  // Stop Service.
  service.stop();
  return 0;
}
