#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include <fstream>
#include <sstream>

#include "3rd_party/marian-dev/src/3rd_party/CLI/CLI.hpp"
#include "3rd_party/yaml-cpp/yaml.h"
#include "common/build_info.h"
#include "common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"
#include "marian.h"

namespace marian {
namespace bergamot {

template <class Service>
struct CLIConfig {
  using ServiceConfig = typename Service::Config;
  using ModelConfigPaths = std::vector<std::string>;

  std::string opMode;

  // For marian-models we retain the old marian-yml configs to a large extent. These are supplied as file-paths to the
  // CLI. For multiple model workflows, we allow more than one model config to be supplied. How to process the models
  // provided is decided by the application.
  ModelConfigPaths modelConfigPaths;

  ServiceConfig serviceConfig;

  /// All config in bergamot has the following templated addOptions(...) method hierarchically placing parse actions on
  /// "option-groups" in nested structs. This allows to keep additional documentation and information on defaults
  /// alongside. Since this is templated with App, we don't add a CLI11 dependency in any configs, thus CLI11 not coming
  /// into the picture until the parser is instantiated.
  template <class App>
  static void addOptions(App &app, CLIConfig<Service> &config, bool multiOpMode = false) {
    if (multiOpMode) {
      app.add_option("--bergamot-mode", config.opMode, "");
    }
    app.add_option("--model-config-paths", config.modelConfigPaths,
                   "Configuration files list, can be used for pivoting multiple models or multiple model workflows");

    ServiceConfig::addOptions(app, config.serviceConfig);
  };
};

/// ConfigParser for bergamot. Internally stores config options with CLIConfig. CLI11 parsing binds the parsing code to
/// write to the members of the CLIConfig instance owned by this class. Usage:
///
/// ```cpp
/// ConfigParser configParser;
/// configParser.parseArgs(argc, argv);
/// auto &config = configParser.getConfig();
/// ```
template <class Service>
class ConfigParser {
 public:
  ConfigParser(const std::string &appName, bool multiOpMode = false) : app_{appName} {
    addSpecialOptions(app_);
    CLIConfig<Service>::addOptions(app_, config_, multiOpMode);
  };
  void parseArgs(int argc, char *argv[]) {
    try {
      app_.parse(argc, argv);
      handleSpecialOptions();
    } catch (const CLI::ParseError &e) {
      exit(app_.exit(e));
    }
  };
  const CLIConfig<Service> &getConfig() { return config_; }

 private:
  // Special Options: build-info and version. These are not taken down further, the respective logic executed and
  // program exits after.
  void addSpecialOptions(CLI::App &app) {
    app.add_flag("--build-info", build_info_, "Print build-info and exit");
    app.add_flag("--version", version_, "Print version-info and exit");
  };

  void handleSpecialOptions() {
    if (build_info_) {
#ifndef _MSC_VER  // cmake build options are not available on MSVC based build.
      std::cerr << cmakeBuildOptionsAdvanced() << std::endl;
      exit(0);
#else   // _MSC_VER
      ABORT("build-info is not available on MSVC based build.");
#endif  // _MSC_VER
    }

    if (version_) {
      std::cerr << buildVersion() << std::endl;
      exit(0);
    }
  }

  CLIConfig<Service> config_;
  CLI::App app_;

  bool build_info_{false};
  bool version_{false};
};

std::shared_ptr<marian::Options> parseOptionsFromString(const std::string &config, bool validate = true,
                                                        std::string pathsInSameDirAs = "");
std::shared_ptr<marian::Options> parseOptionsFromFilePath(const std::string &config, bool validate = true);

}  // namespace bergamot
}  //  namespace marian

#endif  //  SRC_BERGAMOT_PARSER_H
