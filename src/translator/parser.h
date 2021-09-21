#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include <fstream>
#include <sstream>

#include "3rd_party/marian-dev/src/3rd_party/CLI/CLI.hpp"
#include "3rd_party/yaml-cpp/yaml.h"
#include "cache.h"
#include "common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"
#include "marian.h"

namespace marian {
namespace bergamot {

enum OpMode {
  APP_WASM,
  APP_NATIVE,
  APP_DECODER,
  TEST_SOURCE_SENTENCES,
  TEST_TARGET_SENTENCES,
  TEST_SOURCE_WORDS,
  TEST_TARGET_WORDS,
  TEST_QUALITY_ESTIMATOR_WORDS,
  TEST_QUALITY_ESTIMATOR_SCORES,
  TEST_FORWARD_BACKWARD_FOR_OUTBOUND,
  TEST_TRANSLATION_CACHE,
  TEST_BENCHMARK_EDIT_WORKFLOW,
  TEST_CACHE_STORAGE_GROWTH
};

/// Overload for CL11, convert a read from a stringstream into opmode.
std::istringstream &operator>>(std::istringstream &in, OpMode &mode);

struct CLIConfig {
  using ModelConfigPaths = std::vector<std::string>;
  ModelConfigPaths modelConfigPaths;
  bool byteArray;
  bool validateByteArray;
  size_t numWorkers;
  OpMode opMode;

  bool cache;
  CacheConfig cacheConfig;
};

/// ConfigParser for bergamot. Internally stores config options with CLIConfig. CLI11 parsing binds the parsing code to
/// write to the members of the CLIConfig instance owned by this class. Usage:
///
/// ```cpp
/// ConfigParser configParser;
/// configParser.parseArgs(argc, argv);
/// auto &config = configParser.getConfig();
/// ```
class ConfigParser {
 public:
  ConfigParser();
  void parseArgs(int argc, char *argv[]);
  const CLIConfig &getConfig() { return config_; }

 private:
  // Special Options: build-info and version. These are not taken down further, the respective logic executed and
  // program exits after.
  void addSpecialOptions(CLI::App &app);
  void handleSpecialOptions();

  void addOptionsBoundToConfig(CLI::App &app, CLIConfig &config);

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
