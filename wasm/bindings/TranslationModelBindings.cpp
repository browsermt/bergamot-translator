/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "TranslationModel.h"

using namespace emscripten;
/*
std::unique_ptr<std::size_t> createSizeTAlignedMemory() {
  std::cout << "createSizeTAlignedMemory" << std::endl;
  return std::unique_ptr<std::size_t>(new std::size_t(9));
}

EMSCRIPTEN_BINDINGS(aligned_sizet_memory) {
  function("createSizeTAlignedMemory", &createSizeTAlignedMemory);
}

AlignedUint8Memory* createAlignedUint8Memory(std::size_t size, std::size_t alignment) {
  std::cout << "unique ptr createAlignedUint8Memory" << std::endl;
  return new AlignedUint8Memory(size, alignment);
}
*/

val getBytes(marian::bergamot::AlignedMemory& alignedMemory) {
  return val(typed_memory_view(alignedMemory.size(), alignedMemory.as<unsigned char>()));
}

void print(marian::bergamot::AlignedMemory& alignedMemory) {
  std::cout << "Printing contents of memory with size: " << alignedMemory.size() << std::endl;
  for (std::size_t index = 0; index < alignedMemory.size(); index++) {
    std::cout << " Value is: " << alignedMemory[index] << std::endl;
  }
  std::cout << "Done Printing" << std::endl;
}

// Binding code
/*
EMSCRIPTEN_BINDINGS(aligned_uint8_memory) {
  class_<AlignedUint8Memory>("AlignedUint8Memory")
    //.constructor<std::size_t, std::size_t>()
    .constructor(&createAlignedUint8Memory)
    .function("size", &AlignedUint8Memory::size)
    //.function("beginPointer", &marian::bergamot::AlignedVector<void*>::begin, allow_raw_pointers())
	  .function("getBytes", &getBytes)
    .function("print", &print)
    //.function("endVoidPointer", select_overload<void*(void)>(&marian::bergamot::AlignedMemory::end), allow_raw_pointers())
    ;
}
*/

EMSCRIPTEN_BINDINGS(aligned_memory) {
  class_<marian::bergamot::AlignedMemory>("AlignedMemory")
    .constructor<std::size_t, std::size_t>()
    //.constructor(&createAlignedMemory)
    .function("size", &marian::bergamot::AlignedMemory::size)
    //.function("beginPointer", &marian::bergamot::AlignedVector<void*>::begin, allow_raw_pointers())
	  .function("getBytes", &getBytes)
    .function("print", &print)
    //.function("endVoidPointer", select_overload<void*(void)>(&marian::bergamot::AlignedMemory::end), allow_raw_pointers())
    ;
}

TranslationModel* TranslationModelFactory(const std::string &config,
                                          marian::bergamot::AlignedMemory* modelMemory,
                                          marian::bergamot::AlignedMemory* shortlistMemory) {
  print(*modelMemory);
  print(*shortlistMemory);
  //AlignedUint8Memory model = std::move(*modelMemory);
  //AlignedUint8Memory shortList = std::move(*shortlistMemory);
  return new TranslationModel(config, std::move(*modelMemory), std::move(*shortlistMemory));
  //return new TranslationModel(config);
}

EMSCRIPTEN_BINDINGS(translation_model) {
  class_<TranslationModel>("TranslationModel")
    .constructor(&TranslationModelFactory, allow_raw_pointers())
    //.constructor<std::string>()
    .function("translate", &TranslationModel::translate)
	  .function("isAlignmentSupported", &TranslationModel::isAlignmentSupported)
    ;

  register_vector<std::string>("VectorString");
  register_vector<TranslationResult>("VectorTranslationResult");
}
