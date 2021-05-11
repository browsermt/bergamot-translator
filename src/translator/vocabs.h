#pragma once

namespace marian {
namespace bergamot {

class VocabsGenerator {
public:
  // load from buffer
  VocabsGenerator(Ptr<Options> options, std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) {
    ABORT_IF(vocabMemories.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vocabMemories.size());
    for (size_t i = 0; i < vocabs_.size(); i++) {
      Ptr<Vocab> vocab = New<Vocab>(options, i);
      vocab->loadFromSerialized(absl::string_view(vocabMemories[i]->begin(), vocabMemories[i]->size()));
      vocabs_[i] = vocab;
    }
  }

  // load from file
  VocabsGenerator(Ptr<Options> options){
    auto vfiles = options->get<std::vector<std::string>>("vocabs");
    // with the current setup, we need at least two vocabs: src and trg
    ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
    vocabs_.resize(vfiles.size());
    std::unordered_map<std::string, Ptr<Vocab>> vmap;
    for (size_t i = 0; i < vocabs_.size(); ++i) {
      auto m = vmap.emplace(std::make_pair(vfiles[i], Ptr<Vocab>()));
      if (m.second) { // new: load the vocab
        m.first->second = New<Vocab>(options, i);
        m.first->second->load(vfiles[i]);
      }
      vocabs_[i] = m.first->second;
    }
  }

  std::vector<Ptr<Vocab const>> getVocabs(){
    return vocabs_;
  }

private:
  std::vector<Ptr<Vocab const>> vocabs_;
};

std::vector<Ptr<const Vocab>> createVocabs(Ptr<Options> options,
                                           std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories){
  if(!vocabMemories.empty()){
    // load vocabs from buffer
    return VocabsGenerator(options,std::move(vocabMemories)).getVocabs();
  }else {
    // load vocabs from file
    return VocabsGenerator(options).getVocabs();
  }
}

} // namespace bergamot
} // namespace marian
