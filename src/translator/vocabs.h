#pragma once

namespace marian {
namespace bergamot {

/// Wrapper of Marian Vocab objects needed for translator.
/// Holds multiple source vocabularies and one target vocabulary
class Vocabs {
 public:
  /// Construct vocabs object from either byte-arrays or files
  Vocabs(Ptr<Options> options, std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) : options_(options) {
    if (!vocabMemories.empty()) {
      // load vocabs from buffer
      load(std::move(vocabMemories));
    } else {
      // load vocabs from file
      auto vocabPaths = options->get<std::vector<std::string>>("vocabs");
      load(vocabPaths);
    }
  }

  /// Get all source vocabularies (as a vector)
  const std::vector<Ptr<Vocab const>>& sources() const { return srcVocabs_; }

  /// Get the target vocabulary
  const Ptr<Vocab const>& target() const { return trgVocab_; }

 private:
  std::vector<Ptr<Vocab const>> srcVocabs_;  // source vocabularies
  Ptr<Vocab const> trgVocab_;                // target vocabulary
  Ptr<Options> options_;

  // load from buffer
  void load(std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) {
    // At least two vocabs: src and trg
    ABORT_IF(vocabMemories.size() < 2, "Insufficient number of vocabularies.");
    srcVocabs_.resize(vocabMemories.size());
    // hashMap is introduced to avoid double loading the same vocab
    // loading vocabs (either from buffers or files) is the biggest bottleneck of the speed
    // uintptr_t holds unique keys (address) for share_ptr<AlignedMemory>
    std::unordered_map<uintptr_t, Ptr<Vocab>> vmap;
    for (size_t i = 0; i < srcVocabs_.size(); i++) {
      auto m = vmap.emplace(std::make_pair(reinterpret_cast<uintptr_t>(vocabMemories[i].get()), Ptr<Vocab>()));
      if (m.second) {  // new: load the vocab
        m.first->second = New<Vocab>(options_, i);
        m.first->second->loadFromSerialized(absl::string_view(vocabMemories[i]->begin(), vocabMemories[i]->size()));
      }
      srcVocabs_[i] = m.first->second;
    }
    // Initialize target vocab
    trgVocab_ = srcVocabs_.back();
    srcVocabs_.pop_back();
  }

  // load from file
  void load(const std::vector<std::string>& vocabPaths) {
    // with the current setup, we need at least two vocabs: src and trg
    ABORT_IF(vocabPaths.size() < 2, "Insufficient number of vocabularies.");
    srcVocabs_.resize(vocabPaths.size());
    std::unordered_map<std::string, Ptr<Vocab>> vmap;
    for (size_t i = 0; i < srcVocabs_.size(); ++i) {
      auto m = vmap.emplace(std::make_pair(vocabPaths[i], Ptr<Vocab>()));
      if (m.second) {  // new: load the vocab
        m.first->second = New<Vocab>(options_, i);
        m.first->second->load(vocabPaths[i]);
      }
      srcVocabs_[i] = m.first->second;
    }
    // Initialize target vocab
    trgVocab_ = srcVocabs_.back();
    srcVocabs_.pop_back();
  }
};

}  // namespace bergamot
}  // namespace marian
