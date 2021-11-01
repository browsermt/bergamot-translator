#include "parser.h"

#include <unordered_map>

#include "common/build_info.h"
#include "common/config.h"
#include "common/regex.h"
#include "common/version.h"

namespace marian {
namespace bergamot {

std::shared_ptr<marian::Options> parseOptionsFromFilePath(const std::string &configPath, bool validate /*= true*/) {
  // Read entire string and redirect to parseOptionsFromString
  std::ifstream readStream(configPath);
  std::stringstream buffer;
  buffer << readStream.rdbuf();
  return parseOptionsFromString(buffer.str(), validate, /*pathsInSameDirAs=*/configPath);
};

std::shared_ptr<marian::Options> parseOptionsFromString(const std::string &configAsString, bool validate /*= true*/,
                                                        std::string pathsInSameDirAs /*=""*/) {
  marian::Options options;

  marian::ConfigParser configParser(cli::mode::translation);

  // These are additional options we use to hijack for our own marian-replacement layer (for batching,
  // multi-request-compile etc) and hence goes into Ptr<Options>.
  configParser.addOption<size_t>("--max-length-break", "Bergamot Options",
                                 "Maximum input tokens to be processed in a single sentence.", 128);

  // The following is a complete hijack of an existing option, so no need to add explicitly.
  // configParser.addOption<size_t>("--mini-batch-words", "Bergamot Options",
  //                                "Maximum input tokens to be processed in a single sentence.", 1024);

  configParser.addOption<std::string>("--ssplit-prefix-file", "Bergamot Options",
                                      "File with nonbreaking prefixes for sentence splitting.");

  configParser.addOption<std::string>("--ssplit-mode", "Bergamot Options", "[paragraph, sentence, wrapped_text]",
                                      "paragraph");

  configParser.addOption<std::string>("--quality", "Bergamot Options", "File considering Quality Estimation model");

  // Parse configs onto defaultConfig. The preliminary merge sets the YAML internal representation with legal values.
  const YAML::Node &defaultConfig = configParser.getConfig();
  options.merge(defaultConfig);
  options.parse(configAsString);

  // This is in a marian `.cpp` as of now, and requires explicit copy-here.
  // https://github.com/marian-nmt/marian-dev/blob/9fa166be885b025711f27b35453e0f2c00c9933e/src/common/config_parser.cpp#L28

  // clang-format off
  const std::set<std::string> PATHS = {
      "model",
      "models",
      "train-sets",
      "vocabs",
      "embedding-vectors",
      "valid-sets",
      "valid-script-path",
      "valid-script-args",
      "valid-log",
      "valid-translation-output",
      "input",   // except: 'stdin', handled in makeAbsolutePaths and interpolateEnvVars
      "output",  // except: 'stdout', handled in makeAbsolutePaths and interpolateEnvVars
      "pretrained-model",
      "data-weighting",
      "log",
      "sqlite",     // except: 'temporary', handled in the processPaths function
      "shortlist",  // except: only the first element in the sequence is a path, handled in the
                    //  processPaths function
      "ssplit-prefix-file", // added for bergamot
      "quality", // added for bergamot
  };
  // clang-format on

  if (!pathsInSameDirAs.empty()) {
    YAML::Node configYAML = options.cloneToYamlNode();
    marian::cli::makeAbsolutePaths(configYAML, pathsInSameDirAs, PATHS);
    options.merge(configYAML, /*overwrite=*/true);
  }

  // Perform validation on parsed options only when requested
  if (validate) {
    YAML::Node configYAML = options.cloneToYamlNode();
    marian::ConfigValidator validator(configYAML);
    validator.validateOptions(marian::cli::mode::translation);
  }

  return std::make_shared<marian::Options>(options);
}

}  // namespace bergamot
}  // namespace marian
