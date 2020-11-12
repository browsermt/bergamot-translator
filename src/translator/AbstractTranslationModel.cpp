/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

// All 3rd party includes
#include "common/options.h"

// All local includes
#include "AbstractTranslationModel.h"
#include "TranslationModel.h"


std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const TranslationModelConfiguration& config) {
	// ToDo: Write an adaptor for adapting TranslationModelConfiguration to marian::Options
	auto options = std::make_shared<marian::Options>();
	return std::make_shared<TranslationModel>(options);
}
