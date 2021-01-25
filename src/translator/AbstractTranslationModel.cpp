/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

// All 3rd party includes
#include "3rd_party/marian-dev/src/common/options.h"
#include "3rd_party/marian-dev/src/common/config_parser.h"
#include "3rd_party/marian-dev/src/3rd_party/yaml-cpp/yaml.h"

// All local includes
#include "AbstractTranslationModel.h"
#include "TranslationModel.h"


std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const std::string& config) {
	auto options = std::make_shared<marian::Options>(YAML::Load(config));
	marian::ConfigParser configParser(marian::cli::mode::translation);
	const YAML::Node& defaultConfig = configParser.getConfig();
	options->merge(defaultConfig);
	return std::make_shared<TranslationModel>(options);
}
