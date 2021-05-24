#include "cli-framework.h"

int main(int argc, char *argv[]) {
  auto cp = marian::bergamot::createConfigParser();
  auto options = cp.parseOptions(argc, argv, true);
  const std::string mode = options->get<std::string>("bergamot-mode");
  marian::bergamot::execute(mode, options);
  return 0;
}
