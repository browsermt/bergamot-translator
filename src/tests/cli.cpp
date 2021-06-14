
#include "apps.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  const std::string mode = options->get<std::string>("bergamot-mode");
  using namespace marian::bergamot;
  if (mode == "test-response-source-sentences") {
    testapp::annotatedTextSentences(options, /*source=*/true);
  } else if (mode == "test-response-target-sentences") {
    testapp::annotatedTextSentences(options, /*source=*/false);
  } else if (mode == "test-response-source-words") {
    testapp::annotatedTextWords(options, /*source=*/true);
  } else if (mode == "test-response-target-words") {
    testapp::annotatedTextWords(options, /*source=*/false);
  } else {
    ABORT("Unknown --mode {}. Please run a valid test", mode);
  }
  return 0;
}
