/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "response.h"
#include "service.h"

using namespace emscripten;

typedef marian::bergamot::Service TranslationModel;
typedef marian::bergamot::Response TranslationResult;

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
    .function("translate", &TranslationModel::translateMultiple)
	  .function("isAlignmentSupported", &TranslationModel::isAlignmentSupported)
    ;
  // ^ We redirect Service::translateMultiple to WASMBound::translate instead. Sane API is
  // translate. If and when async comes, we can be done with this inconsistency.

  register_vector<std::string>("VectorString");
  register_vector<TranslationResult>("VectorTranslationResult");
}
