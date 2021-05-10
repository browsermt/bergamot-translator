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

    register_vector<marian::bergamot::AlignedMemory*>("AlignedMemoryList");
}

std::vector<std::shared_ptr<marian::bergamot::AlignedMemory>>
prepareVocabsSmartMemories(std::vector<marian::bergamot::AlignedMemory*>& vocabsMemories) {
  auto sourceVocabMemory = std::make_shared<marian::bergamot::AlignedMemory>(std::move(*(vocabsMemories[0])));
  std::vector<std::shared_ptr<marian::bergamot::AlignedMemory>> vocabsSmartMemories;
  vocabsSmartMemories.push_back(sourceVocabMemory);
  // When source and target vocab files are same, only one memory object is passed in vocabsMemories
  // to avoid double memory allocation for the same file. However, the constructor of the TranslationModel
  // class still expects 2 entries where each entry has the shared ownership of a single AlignedMemory object.
  if (vocabsMemories.size() == 2) {
    auto targetVocabMemory = std::make_shared<marian::bergamot::AlignedMemory>(std::move(*(vocabsMemories[1])));
    vocabsSmartMemories.push_back(std::move(targetVocabMemory));
  }
  else {
    vocabsSmartMemories.push_back(sourceVocabMemory);
  }
  return vocabsSmartMemories;
}

TranslationModel* TranslationModelFactory(const std::string &config,
                                          marian::bergamot::AlignedMemory* modelMemory,
                                          marian::bergamot::AlignedMemory* shortlistMemory,
                                          std::vector<marian::bergamot::AlignedMemory*> uniqueVocabsMemories) {
  return new TranslationModel(config,
                              std::move(*modelMemory),
                              std::move(*shortlistMemory),
                              std::move(prepareVocabsSmartMemories(uniqueVocabsMemories)));
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
