/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "TranslationModel.h"

using namespace emscripten;

val getByteArrayView(marian::bergamot::AlignedMemory& alignedMemory) {
  return val(typed_memory_view(alignedMemory.size(), alignedMemory.as<char>()));
}

EMSCRIPTEN_BINDINGS(aligned_memory) {
  class_<marian::bergamot::AlignedMemory>("AlignedMemory")
    .constructor<std::size_t, std::size_t>()
    .function("size", &marian::bergamot::AlignedMemory::size)
	  .function("getByteArrayView", &getByteArrayView)
    ;
}

TranslationModel* TranslationModelFactory(const std::string &config,
                                          marian::bergamot::AlignedMemory* modelMemory,
                                          marian::bergamot::AlignedMemory* shortlistMemory) {
  return new TranslationModel(config, std::move(*modelMemory), std::move(*shortlistMemory));
}

EMSCRIPTEN_BINDINGS(translation_model) {
  class_<TranslationModel>("TranslationModel")
    .constructor(&TranslationModelFactory, allow_raw_pointers())
    .function("translate", &TranslationModel::translate)
	  .function("isAlignmentSupported", &TranslationModel::isAlignmentSupported)
    ;

  register_vector<std::string>("VectorString");
  register_vector<TranslationResult>("VectorTranslationResult");
}
