#include "cli.h"

int main(int argc, char *argv[]) {
  marian::bergamot::ConfigParser configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();
  using namespace marian::bergamot;
  switch (config.opMode) {
    case OpMode::APP_WASM:
      app::wasm(config);
      break;
    case OpMode::APP_NATIVE:
      app::native(config);
      break;
    case OpMode::APP_DECODER:
      app::decoder(config);
      break;
    default:
      break;
  }
  return 0;
}
