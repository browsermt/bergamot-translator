#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include "3rd_party/yaml-cpp/yaml.h"
#include "common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"
#include "marian.h"

namespace marian {
namespace bergamot {

inline marian::ConfigParser createConfigParser() {
  marian::ConfigParser cp(marian::cli::mode::translation);
  cp.addOption<std::string>(
      "--ssplit-prefix-file", "Bergamot Options",
      "File with nonbreaking prefixes for sentence splitting.");

  cp.addOption<std::string>("--ssplit-mode", "Server Options",
                            "[paragraph, sentence, wrapped_text]", "paragraph");

  cp.addOption<int>(
      "--max-length-break", "Bergamot Options",
      "Maximum input tokens to be processed in a single sentence.", 128);

  cp.addOption<bool>(
      "--check-bytearray", "Bergamot Options",
      "Flag holds whether to check the content of the bytearray (true by default)", true);

    return cp;
}

inline std::shared_ptr<marian::Options>
parseOptions(const std::string &config, bool validate = true) {
  marian::Options options;

  // @TODO(jerinphilip) There's something off here, @XapaJIaMnu suggests
  // that should not be using the defaultConfig. This function only has access
  // to std::string config and needs to be able to construct Options from the
  // same.

  // Absent the following code-segment, there is a parsing exception thrown on
  // rebuilding YAML.
  //
  // Error: Unhandled exception of type 'N4YAML11InvalidNodeE': invalid node;
  // this may result from using a map iterator as a sequence iterator, or
  // vice-versa
  //
  // Error: Aborted from void unhandledException() in
  // 3rd_party/marian-dev/src/common/logging.cpp:113

  marian::ConfigParser configParser = createConfigParser();
  const YAML::Node &defaultConfig = configParser.getConfig();

  options.merge(defaultConfig);

  // Parse configs onto defaultConfig.
  options.parse(config);
  YAML::Node configCopy = options.cloneToYamlNode();

  if (validate) {
    // Perform validation on parsed options only when requested
    marian::ConfigValidator validator(configCopy);
    validator.validateOptions(marian::cli::mode::translation);
  }

  return std::make_shared<marian::Options>(options);
}

} //  namespace bergamot
} //  namespace marian

#endif //  SRC_BERGAMOT_PARSER_H
