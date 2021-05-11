#pragma once

namespace marian {
namespace bergamot {

class VocabsGenerator {
public:
  VocabsGenerator(Ptr<Options> options): options_(options){}

  // load from buffer
  void load(std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) {
    // At least two vocabs: src and trg
    ABORT_IF(vocabMemories.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vocabMemories.size());
    // hashMap is introduced to avoid double loading the same vocab
    // loading vocabs (either from buffers or files) is the biggest bottleneck of the speed
    // uintptr_t holds unique keys (address) for share_ptr<AlignedMemory>
    std::unordered_map<uintptr_t, Ptr<Vocab>> vmap;
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
  void load(const std::vector<std::string>& vocabPaths){
    // with the current setup, we need at least two vocabs: src and trg
    ABORT_IF(vocabPaths.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vocabPaths.size());
    std::unordered_map<std::string, Ptr<Vocab>> vmap;
    for (size_t i = 0; i < vocabs_.size(); ++i) {
      auto m = vmap.emplace(std::make_pair(vocabPaths[i], Ptr<Vocab>()));
      if (m.second) { // new: load the vocab
        m.first->second = New<Vocab>(options_, i);
        m.first->second->load(vocabPaths[i]);
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
    auto vocabPaths = options->get<std::vector<std::string>>("vocabs");
    vocabsGenerator.load(vocabPaths);
  }
  return vocabsGenerator.getVocabs();
}

} // namespace bergamot
} // namespace marian
