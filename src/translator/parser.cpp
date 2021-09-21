#include "parser.h"

#include <unordered_map>

#include "common/build_info.h"
#include "common/config.h"
#include "common/regex.h"
#include "common/version.h"

namespace marian {
namespace bergamot {

std::istringstream &operator>>(std::istringstream &in, OpMode &mode) {
  std::string modeString;
  in >> modeString;
  std::unordered_map<std::string, OpMode> table = {
      {"wasm", OpMode::APP_WASM},
      {"native", OpMode::APP_NATIVE},
      {"decoder", OpMode::APP_DECODER},
      {"test-response-source-sentences", OpMode::TEST_SOURCE_SENTENCES},
      {"test-response-target-sentences", OpMode::TEST_TARGET_SENTENCES},
      {"test-response-source-words", OpMode::TEST_SOURCE_WORDS},
      {"test-response-target-words", OpMode::TEST_TARGET_WORDS},
      {"test-quality-estimator-words", OpMode::TEST_QUALITY_ESTIMATOR_WORDS},
      {"test-quality-estimator-scores", OpMode::TEST_QUALITY_ESTIMATOR_SCORES},
      {"test-forward-backward", OpMode::TEST_FORWARD_BACKWARD_FOR_OUTBOUND},
      {"test-translation-cache", OpMode::TEST_TRANSLATION_CACHE},
      {"test-benchmark-edit-workflow", OpMode::TEST_BENCHMARK_EDIT_WORKFLOW},
      {"test-cache-storage-growth", OpMode::TEST_CACHE_STORAGE_GROWTH}
  };

  auto query = table.find(modeString);
  if (query != table.end()) {
    mode = query->second;
  } else {
    ABORT("Unknown mode {}", modeString);
  }

  return in;
}

ConfigParser::ConfigParser() : app_{"Bergamot Options"} {
  addSpecialOptions(app_);
  addOptionsBoundToConfig(app_, config_);
};

void ConfigParser::parseArgs(int argc, char *argv[]) {
  try {
    app_.parse(argc, argv);
    handleSpecialOptions();
  } catch (const CLI::ParseError &e) {
    exit(app_.exit(e));
  }
}

void ConfigParser::addSpecialOptions(CLI::App &app) {
  app.add_flag("--build-info", build_info_, "Print build-info and exit");
  app.add_flag("--version", version_, "Print version-info and exit");
}

void ConfigParser::handleSpecialOptions() {
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

void ConfigParser::addOptionsBoundToConfig(CLI::App &app, CLIConfig &config) {
  app.add_option("--model-config-paths", config.modelConfigPaths,
                 "Configuration files list, can be used for pivoting multiple models or multiple model workflows");

  app.add_flag("--bytearray", config.byteArray,
               "Flag holds whether to construct service from bytearrays, only for testing purpose");

  app.add_flag("--check-bytearray", config.validateByteArray,
               "Flag holds whether to check the content of the bytearrays (true by default)");

  app.add_option("--cpu-threads", config.numWorkers, "Number of worker threads to use for translation");

  app_.add_option("--bergamot-mode", config.opMode, "Operating mode for bergamot: [wasm, native, decoder]");

  app_.add_option("--cache-translations", config.cache, "To cache translations or not", true);

  auto &cacheConfig = config.cacheConfig;
  app_.add_option("--cache-size", cacheConfig.size, "Number of histories to keep in translation cache", 20);
  app_.add_option("--cache-ebr-queue-size", cacheConfig.ebrQueueSize,
                  "Number of action to allow in a queue for epoch based reclamation.", 1000);
  app_.add_option("--cache-ebr-num-queues", cacheConfig.ebrNumQueues,
                  "Number of queues of actions to maintain, increase to increase throughput.", 4);
  app_.add_option("--cache-ebr-interval", cacheConfig.ebrIntervalInMilliseconds,
                  "Time between runs of background thread for epoch-based-reclamation, in milliseconds.", 1000 /*ms*/);
  app_.add_option("--cache-buckets", cacheConfig.numBuckets, "Number of buckets to keep in the hashtable", 10000);
  app_.add_option("--cache-remove-expired", cacheConfig.removeExpired,
                  "Whether to remove expired records on a garbage collection iteration or not. Expiry determined by "
                  "user-specified time-window",
                  false);
  app_.add_option("--cache-time-to-live", cacheConfig.timeToLiveInMilliseconds,
                  "How long a record in cache should be valid for in milliseconds. This is insignificant without "
                  "remove expired flag set.",
                  1000 /*ms*/);
}

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
