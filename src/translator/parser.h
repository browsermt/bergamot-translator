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

struct CLIConfig {
  using ModelConfigs = std::vector<std::string>;
  ModelConfigs modelConfigs;
  std::string ssplitPrefixFilePath;
  std::string ssplitMode;
  size_t maxLengthBreak;
  size_t miniBatchWords;

  bool byteArray;
  bool validateByteArray;

  size_t numWorkers;
  OpMode opMode;
};

class ConfigParser {
 public:
  ConfigParser();
  void parseArgs(int argc, char *argv[]);
  void addOptionsBoundToConfig(CLI::App &app, CLIConfig &config);
  void addSpecialOptions(CLI::App &app);
  void handleSpecialOptions();
  const CLIConfig &getConfig() { return config_; }

 private:
  CLIConfig config_;
  CLI::App app_;

  bool build_info_{false};
  bool version_{false};
};

std::shared_ptr<marian::Options> parseOptionsFromFilePath(const std::string &configPath, bool validate = true);

std::shared_ptr<marian::Options> parseOptionsFromString(const std::string &config, bool validate = true);

}  //  namespace bergamot
}  //  namespace marian

#endif  //  SRC_BERGAMOT_PARSER_H
