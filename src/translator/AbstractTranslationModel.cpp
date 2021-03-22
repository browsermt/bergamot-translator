/*
 * AbstractTranslationModel.cpp
 *
 */
#include <memory>

// All local includes
#include "AbstractTranslationModel.h"
#include "TranslationModel.h"

/**
 * @param config Marian yml config file in the form of a string
 * @param model_memory byte array (aligned to 64!!!) that contains the bytes of a model.bin. Optional, defaults to nullptr when not used
 */
std::shared_ptr<AbstractTranslationModel>
AbstractTranslationModel::createInstance(const std::string &config, const void * model_memory) {
  return std::make_shared<TranslationModel>(config, model_memory);
}
