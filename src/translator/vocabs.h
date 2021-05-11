#pragma once

#include "common/timer.h"

namespace marian {
namespace bergamot {

class VocabsGenerator {
public:
  VocabsGenerator(Ptr<Options> options): options_(options){}

  // load from buffer
  void load(std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) {
    // with the current setup, we need at least two vocabs: src and trg
    ABORT_IF(vocabMemories.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vocabMemories.size());
    std::unordered_map<uintptr_t, Ptr<Vocab>> vmap;  // uintptr_t holds unique keys for share_ptr<AlignedMemory>
    for (size_t i = 0; i < vocabs_.size(); i++) {
      auto m = vmap.emplace(std::make_pair(reinterpret_cast<uintptr_t>(vocabMemories[i].get()), Ptr<Vocab>()));
      if (m.second) { // new: load the vocab
        m.first->second = New<Vocab>(options_, i);
        m.first->second->loadFromSerialized(absl::string_view(vocabMemories[i]->begin(), vocabMemories[i]->size()));
      }
      vocabs_[i] = m.first->second;
    }
  }

  // load from file
  void load(const std::vector<std::string>& vocabPath){
    // with the current setup, we need at least two vocabs: src and trg
    ABORT_IF(vocabPath.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vocabPath.size());
    std::unordered_map<std::string, Ptr<Vocab>> vmap;
    for (size_t i = 0; i < vocabs_.size(); ++i) {
      auto m = vmap.emplace(std::make_pair(vocabPath[i], Ptr<Vocab>()));
      if (m.second) { // new: load the vocab
        m.first->second = New<Vocab>(options_, i);
        m.first->second->load(vocabPath[i]);
      }
      vocabs_[i] = m.first->second;
    }
  }

  std::vector<Ptr<Vocab const>> getVocabs(){
    return vocabs_;
  }

private:
  std::vector<Ptr<Vocab const>> vocabs_;
  Ptr<Options> options_;
};

std::vector<Ptr<const Vocab>> loadVocabs(Ptr<Options> options,
                                         std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories){
  VocabsGenerator vocabsGenerator(options);
  if(!vocabMemories.empty()){
    vocabsGenerator.load(std::move(vocabMemories));  // load vocabs from buffer
  }else {
    // load vocabs from file
    auto vocabFiles = options->get<std::vector<std::string>>("vocabs");
    vocabsGenerator.load(vocabFiles);
  }
  return vocabsGenerator.getVocabs();
}

} // namespace bergamot
} // namespace marian
