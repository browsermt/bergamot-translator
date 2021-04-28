/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "TranslationModel.h"

using namespace emscripten;

val getBytes(marian::bergamot::AlignedMemory& alignedMemory) {
  return val(typed_memory_view(alignedMemory.size(), alignedMemory.as<char>()));
}

void print(marian::bergamot::AlignedMemory& alignedMemory) {
  std::cout << "Printing memory with size: " << alignedMemory.size() << std::endl;
  /*for (std::size_t index = 0; index < alignedMemory.size(); index++) {
    std::cout << alignedMemory[index];
  }
  std::cout << std::endl;*/
  std::cout << "Done Printing" << std::endl;
}

EMSCRIPTEN_BINDINGS(aligned_memory) {
  class_<marian::bergamot::AlignedMemory>("AlignedMemory")
    .constructor<std::size_t, std::size_t>()
    .function("size", &marian::bergamot::AlignedMemory::size)
	  .function("getBytes", &getBytes)
    .function("print", &print)
    ;
}

TranslationModel* TranslationModelFactory(const std::string &config,
                                          marian::bergamot::AlignedMemory* modelMemory,
                                          marian::bergamot::AlignedMemory* shortlistMemory = nullptr) {
  std::cout << "modelMemory size before moving: " << modelMemory->size() << std::endl;
  TranslationModel* model = nullptr;
  if (shortlistMemory != nullptr) {
    std::cout << "shortlistMemory size before moving: " << shortlistMemory->size() << std::endl;
    model = new TranslationModel(config, std::move(*modelMemory), std::move(*shortlistMemory));
  }
  else {
    model = new TranslationModel(config, std::move(*modelMemory));
  }

  std::cout << "modelMemory size after moving: " << modelMemory->size() << std::endl;
  if (shortlistMemory != nullptr)
    std::cout << "shortlistMemory size after moving: " << shortlistMemory->size() << std::endl;
  return model;
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
