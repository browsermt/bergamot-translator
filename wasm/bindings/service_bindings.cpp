/*
 * Bindings for Service class
 */

#include <emscripten/bind.h>

#include "service.h"

using namespace emscripten;

using BlockingService = marian::bergamot::BlockingService;
using AlignedMemory = marian::bergamot::AlignedMemory;
using MemoryBundle = marian::bergamot::MemoryBundle;

val getByteArrayView(AlignedMemory& alignedMemory) {
  return val(typed_memory_view(alignedMemory.size(), alignedMemory.as<char>()));
}

EMSCRIPTEN_BINDINGS(aligned_memory) {
  class_<AlignedMemory>("AlignedMemory")
      .constructor<std::size_t, std::size_t>()
      .function("size", &AlignedMemory::size)
      .function("getByteArrayView", &getByteArrayView);

  register_vector<AlignedMemory*>("AlignedMemoryList");
}

// When source and target vocab files are same, only one memory object is passed from JS to
// avoid allocating memory twice for the same file. However, the constructor of the Service
// class still expects 2 entries in this case, where each entry has the shared ownership of the
// same AlignedMemory object. This function prepares these smart pointer based AlignedMemory objects
// for unique AlignedMemory objects passed from JS.
std::vector<std::shared_ptr<AlignedMemory>> prepareVocabsSmartMemories(std::vector<AlignedMemory*>& vocabsMemories) {
  auto sourceVocabMemory = std::make_shared<AlignedMemory>(std::move(*(vocabsMemories[0])));
  std::vector<std::shared_ptr<AlignedMemory>> vocabsSmartMemories;
  vocabsSmartMemories.push_back(sourceVocabMemory);
  if (vocabsMemories.size() == 2) {
    auto targetVocabMemory = std::make_shared<AlignedMemory>(std::move(*(vocabsMemories[1])));
    vocabsSmartMemories.push_back(std::move(targetVocabMemory));
  } else {
    vocabsSmartMemories.push_back(sourceVocabMemory);
  }
  return vocabsSmartMemories;
}

MemoryBundle prepareMemoryBundle(AlignedMemory* modelMemory, AlignedMemory* shortlistMemory,
                                 std::vector<AlignedMemory*> uniqueVocabsMemories) {
  MemoryBundle memoryBundle;
  memoryBundle.model = std::move(*modelMemory);
  memoryBundle.shortlist = std::move(*shortlistMemory);
  memoryBundle.vocabs = std::move(prepareVocabsSmartMemories(uniqueVocabsMemories));

  return memoryBundle;
}

EMSCRIPTEN_BINDINGS(translation_model) {
  class_<TranslationModel>("TranslationModel", &std::make_shared<TranslationModel>);
}

EMSCRIPTEN_BINDINGS(blocking_service) {
  class_<BlockingService>("BlockingService")
      .constructor()
      .function("createCompatibleModel",
                [](BlockingService& self, AlignedMemory* modelMemory, AlignedMemory* shortlistMemory,
                   std::vector<AlignedMemory*> uniqueVocabsMemories) {
                  MemoryBundle memoryBundle = prepareMemoryBundle(modelMemory, shortlistMemory, uniqueVocabsMemories);
                  return self.createCompatibleModel(config, std::move(memoryBundle));
                })
      .function("translate", &BlockingService::translateMultiple);

  register_vector<std::string>("VectorString");
}
