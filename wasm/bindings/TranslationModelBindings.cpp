/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "TranslationModel.h"

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_model) {
  class_<TranslationModel>("TranslationModel")
    .constructor<std::string>()
    .function("translate", &TranslationModel::translate)
	  .function("isAlignmentSupported", &TranslationModel::isAlignmentSupported)
    ;

  register_vector<std::string>("VectorString");
  register_vector<TranslationResult>("VectorTranslationResult");
}
