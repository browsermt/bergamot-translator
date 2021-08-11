#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include <fstream>
#include <sstream>

#include "3rd_party/marian-dev/src/3rd_party/CLI/CLI.hpp"
#include "3rd_party/yaml-cpp/yaml.h"
#include "common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"
#include "marian.h"

namespace marian {
namespace bergamot {

enum OpMode {
  WASM,
  NATIVE,
  DECODER,
  TEST_SOURCE_SENTENCES,
  TEST_TARGET_SENTENCES,
  TEST_SOURCE_WORDS,
  TEST_TARGET_WORDS,
};

std::istringstream &operator>>(std::istringstream &in, OpMode &mode);

struct CLIConfig {
  using ModelConfigPaths = std::vector<std::string>;
  ModelConfigPaths modelConfigPaths;
  bool byteArray;
  bool validateByteArray;
  size_t numWorkers;
  OpMode opMode;
};

class ConfigParser {
 public:
  ConfigParser();
  void parseArgs(int argc, char *argv[]);
  const CLIConfig &getConfig() { return config_; }

 private:
  void addOptionsBoundToConfig(CLI::App &app, CLIConfig &config);
  void addSpecialOptions(CLI::App &app);
  void handleSpecialOptions();

  CLIConfig config_;
  CLI::App app_;

  bool build_info_{false};
  bool version_{false};
};

std::shared_ptr<marian::Options> parseOptionsFromString(const std::string &config, bool validate = true,
                                                        std::string pathsInSameDirAs = "");
std::shared_ptr<marian::Options> parseOptionsFromFilePath(const std::string &config, bool validate = true);

}  //  namespace bergamot
}  //  namespace marian

#endif  //  SRC_BERGAMOT_PARSER_H
