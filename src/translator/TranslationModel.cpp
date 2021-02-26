/*
 * TranslationModel.cpp
 *
 */

#include <future>
#include <vector>

// All 3rd party includes
#include "3rd_party/marian-dev/src/3rd_party/yaml-cpp/yaml.h"
#include "3rd_party/marian-dev/src/common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"

// All local project includes
#include "TranslationModel.h"
#include "translator/parser.h"
#include "translator/service_base.h"

std::shared_ptr<marian::Options> parseOptions(const std::string &config) {
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

  marian::ConfigParser configParser = marian::bergamot::createConfigParser();
  const YAML::Node &defaultConfig = configParser.getConfig();

  options.merge(defaultConfig);

  // Parse configs onto defaultConfig.
  options.parse(config);
  YAML::Node configCopy = options.cloneToYamlNode();

  marian::ConfigValidator validator(configCopy);
  validator.validateOptions(marian::cli::mode::translation);

  return std::make_shared<marian::Options>(options);
}

TranslationModel::TranslationModel(const std::string &config)
    : configOptions_(std::move(parseOptions(config))),
      AbstractTranslationModel(), service_(configOptions_) {}

TranslationModel::~TranslationModel() {}

std::vector<TranslationResult>
TranslationModel::translate(std::vector<std::string> &&texts,
                            TranslationRequest request) {
  // Implementing a non-async version first. Unpleasant, but should work.
  std::promise<std::vector<TranslationResult>> promise;
  auto future = promise.get_future();

  // This code, move into async?
  std::vector<TranslationResult> translationResults;
  for (auto &text : texts) {
    // Collect future as marian::bergamot::TranslationResult
    auto intermediate = service_.translate(std::move(text));
    intermediate.wait();
    auto marianResponse(std::move(intermediate.get()));

    // This mess because marian::string_view != std::string_view
    std::string source, translation;
    marian::bergamot::Response::SentenceMappings mSentenceMappings;
    marianResponse.move(source, translation, mSentenceMappings);

    // Convert to UnifiedAPI::TranslationResult
    TranslationResult::SentenceMappings sentenceMappings;
    for (auto &p : mSentenceMappings) {
      std::string_view src(p.first.data(), p.first.size()),
          tgt(p.second.data(), p.second.size());
      sentenceMappings.emplace_back(src, tgt);
    }

    // In place construction.
    translationResults.emplace_back(
        std::move(source),          // &&marianResponse.source_
        std::move(translation),     // &&marianResponse.translation_
        std::move(sentenceMappings) // &&sentenceMappings
    );
  }

  return translationResults;
}

bool TranslationModel::isAlignmentSupported() const { return false; }
