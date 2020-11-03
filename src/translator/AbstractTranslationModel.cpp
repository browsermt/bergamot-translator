/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

#include "AbstractTranslationModel.h"
#include "TranslationModel.h"


std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const TranslationModelConfiguration& config) {
	return std::make_shared<TranslationModel>(config);
}
