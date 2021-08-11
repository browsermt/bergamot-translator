#include "apps.h"

int main(int argc, char *argv[]) {
  using namespace marian::bergamot;
  marian::bergamot::ConfigParser configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();
  AsyncService service(config.numWorkers);
  auto modelConfig = parseOptionsFromFilePath(config.modelConfigPaths.front());

  switch (config.opMode) {
    case OpMode::TEST_SOURCE_SENTENCES:
      testapp::annotatedTextSentences(service, modelConfig, /*source=*/true);
      break;
    case OpMode::TEST_TARGET_SENTENCES:
      testapp::annotatedTextSentences(service, modelConfig, /*source=*/false);
      break;
    case OpMode::TEST_SOURCE_WORDS:
      testapp::annotatedTextWords(service, modelConfig, /*source=*/true);
      break;
    case OpMode::TEST_TARGET_WORDS:
      testapp::annotatedTextWords(service, modelConfig, /*source=*/false);
      break;
    default:
      ABORT("Incompatible op-mode. Choose one of the test modes.");
      break;
  }
  return 0;
}
