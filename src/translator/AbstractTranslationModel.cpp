/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

// All local includes
#include "AbstractTranslationModel.h"
#include "TranslationModel.h"

std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const std::string &config) {
  return std::make_shared<TranslationModel>(config);
}
