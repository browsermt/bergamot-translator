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
  cp.addOption<std::string>("--ssplit-prefix-file", "Bergamot Options",
                            "File with nonbreaking prefixes for sentence splitting.");

  cp.addOption<std::string>("--ssplit-mode", "Server Options", "[paragraph, sentence, wrapped_text]", "paragraph");

  cp.addOption<int>("--max-length-break", "Bergamot Options",
                    "Maximum input tokens to be processed in a single sentence.", 128);

  cp.addOption<bool>("--bytearray", "Bergamot Options",
                     "Flag holds whether to construct service from bytearrays, only for testing purpose", false);

  cp.addOption<bool>("--check-bytearray", "Bergamot Options",
                     "Flag holds whether to check the content of the bytearrays (true by default)", true);

  cp.addOption<std::string>("--bergamot-mode", "Bergamot Options",
                            "Operating mode for bergamot: [wasm, native, decoder]", "native");

  // clang-format off
  
  cp.addOption<bool  >("--cache-translations",   "Bergamot Cache Options", "To cache translations or not", true);
  cp.addOption<size_t>("--cache-size",           "Bergamot Cache Options", "Number of histories to keep in translation cache", 20);
  cp.addOption<size_t>("--cache-ebr-queue-size", "Bergamot Cache Options", "Number of action to allow in a queue for epoch based reclamation.", 1000);
  cp.addOption<size_t>("--cache-ebr-num-queues", "Bergamot Cache Options", "Number of queues of actions to maintain, increase to increase throughput.", 4);
  cp.addOption<size_t>("--cache-ebr-interval",   "Bergamot Cache Options", "Time between runs of background thread for epoch-based-reclamation, in milliseconds.", 1000/*ms*/);
  cp.addOption<size_t>("--cache-buckets",        "Bergamot Cache Options", "Number of buckets to keep in the hashtable", 10000);
  cp.addOption<bool  >("--cache-remove-expired", "Bergamot Cache Options", "Whether to remove expired records on a garbage collection iteration or not. Expiry determined by user-specified time-window", false);
  cp.addOption<size_t>("--cache-time-to-live",   "Bergamot Cache Options", "How long a record in cache should be valid for in milliseconds. This is insignificant without remove expired flag set.", 1000/*ms*/);

  // clang-format on

  return cp;
}

inline std::shared_ptr<marian::Options> parseOptions(const std::string &config, bool validate = true) {
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

}  //  namespace bergamot
}  //  namespace marian

#endif  //  SRC_BERGAMOT_PARSER_H
