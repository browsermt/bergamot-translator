#include "cli.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  const std::string mode = options->get<std::string>("bergamot-mode");
  using namespace marian::bergamot;
  if (mode == "wasm") {
    app::wasm(options);
  } else if (mode == "native") {
    app::native(options);
  } else if (mode == "decoder") {
    app::decoder(options);
  } else {
    ABORT("Unknown --mode {}. Use one of: {wasm,native,decoder}", mode);
  }
  return 0;
}
