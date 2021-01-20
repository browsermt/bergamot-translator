#include <cstdlib>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/history.h"
#include "translator/output_collector.h"
#include "translator/output_printer.h"

#include "translator/service.h"

int main(int argc, char *argv[]) {
  marian::ConfigParser cp(marian::cli::mode::translation);

  cp.addOption<std::string>(
      "--ssplit-prefix-file", "Bergamot Options",
      "File with nonbreaking prefixes for sentence splitting.");

  cp.addOption<std::string>("--ssplit-mode", "Server Options",
                            "[paragraph, sentence, wrapped_text]");

  cp.addOption<int>(
      "--max-input-sentence-tokens", "Bergamot Options",
      "Maximum input tokens to be processed in a single sentence.", 128);

  cp.addOption<int>("--max-input-tokens", "Bergamot Options",
                    "Maximum input tokens in a batch. control for"
                    "Bergamot Queue",
                    1024);

  // Launch service.
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
